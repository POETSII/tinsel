// This is a cut-down version of Ichiro Kawazome's udmabuf driver
// (https://github.com/ikwzm/udmabuf).  It allocates a number of DMA buffers
// using Linux's DMA API and allows these buffers to be mmapped to user space.
// One device file is created for each buffer.  The physical address of a
// buffer (to be used by a device) can be obtained using an ioctl on the
// corresponding device file.  The number of buffers, and their sizes, are
// compile-time options.

/******************************************************************************
 *
 * Copyright (C) 2015-2017 Ichiro Kawazome
 * Copyright (C) 2017 Matthew Naylor
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 *   1. Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 * 
 *   2. Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in
 *      the documentation and/or other materials provided with the
 *      distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT 
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * 
 *****************************************************************************/

#include <linux/cdev.h>
#include <linux/dma-mapping.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/device.h>
#include <asm/uaccess.h>

// Constants
// =========

// Number of DMA buffers to create
// (Also the number of device files to create)
#define NUM_BUFFERS 4

// Size of each buffer in bytes
#define SIZE 1048576

// Types
// =====

struct dmabuffer_state_t {
  struct device*  device;
  struct cdev     cdev;
  int             cdev_valid;
  dev_t           device_number;
  int             size;
  size_t          alloc_size;
  void*           virt_addr;
  dma_addr_t      phys_addr;
  u64             dma_mask;
};

// Global state
// ============

// Base device number
static dev_t dmabuffer_base_devnum;

// Sys class
static struct class* dmabuffer_sys_class;

// State for each device
static struct dmabuffer_state_t dmabuffer_state[NUM_BUFFERS];

// File operations
// ===============

static int dmabuffer_file_open(struct inode *inode, struct file *file)
{
  struct dmabuffer_state_t* state;

  state = container_of(inode->i_cdev, struct dmabuffer_state_t, cdev);
  file->private_data = state;

  return 0;
}

static int dmabuffer_file_release(struct inode *inode, struct file *file)
{
  return 0;
}

static long dmabuffer_file_ioctl(struct file *file,
             unsigned int ioctl_num, unsigned long ioctl_param)
{
  u64* ptr = (u64*) ioctl_param;
  struct dmabuffer_state_t* state = file->private_data;
  put_user(state->phys_addr, ptr);
  return 0;
}

static int dmabuffer_file_mmap(struct file *file, struct vm_area_struct* vma)
{
  struct dmabuffer_state_t* state = file->private_data;
  return dma_mmap_coherent(state->device, vma, state->virt_addr,
                             state->phys_addr, state->alloc_size);
}

static const struct file_operations dmabuffer_fops = {
  .owner          = THIS_MODULE,
  .open           = dmabuffer_file_open,
  .release        = dmabuffer_file_release,
  .unlocked_ioctl = dmabuffer_file_ioctl,
  .mmap           = dmabuffer_file_mmap,
};

// Module init & exit
// ==================

static void __exit dmabuffer_module_exit(void)
{
  // Free components for each device
  int d;
  for (d = 0; d < NUM_BUFFERS; d++) {
    struct dmabuffer_state_t* state = &dmabuffer_state[d];
    // Free buffer
    if (state->virt_addr != NULL)
      dma_free_coherent(state->device, state->alloc_size,
                          state->virt_addr, state->phys_addr);
    // Free device
    if (state->device != NULL)
      device_destroy(dmabuffer_sys_class, state->device_number);
    // Remove char device
    if (state->cdev_valid)
      cdev_del(&state->cdev);
  }

  // Free sys class
  if (dmabuffer_sys_class != NULL)
    class_destroy(dmabuffer_sys_class);

  // Free device numbers
  if (dmabuffer_base_devnum != 0)
    unregister_chrdev_region(dmabuffer_base_devnum, NUM_BUFFERS);
}

static int __init dmabuffer_module_init(void)
{
  int retval, d, dma_mask_bit;
  dev_t major;

  // Allocate device numbers
  retval = alloc_chrdev_region(&dmabuffer_base_devnum, 0,
             NUM_BUFFERS, "dmabuffer");
  if (retval != 0) {
    printk(KERN_ERR "dmabuffer: couldn't allocate device numbers\n");
    dmabuffer_base_devnum = 0;
    return retval;
  }

  // Extract major device number
  major = MAJOR(dmabuffer_base_devnum);

  //  Create device class
  dmabuffer_sys_class = class_create(THIS_MODULE, "dmabuffer");
  if (IS_ERR_OR_NULL(dmabuffer_sys_class)) {
    printk(KERN_ERR "dmabuffer: couldn't create sys class\n");
    dmabuffer_sys_class = NULL;
    dmabuffer_module_exit();
    return PTR_ERR(dmabuffer_sys_class);
  }

  // Initialise each device
  for (d = 0; d < NUM_BUFFERS; d++) {
    struct dmabuffer_state_t* state = &dmabuffer_state[d];

    // Set device number
    state->device_number = MKDEV(major, d);

    // Compute size of allocation
    state->alloc_size =
      ((SIZE + ((1 << PAGE_SHIFT) - 1)) >> PAGE_SHIFT) << PAGE_SHIFT;

    // Create device
    state->device = device_create(dmabuffer_sys_class,
                                   NULL, state->device_number,
                                     (void*) state, "dmabuffer%d", d);
    if (IS_ERR_OR_NULL(state->device)) {
      state->device = NULL;
      dmabuffer_module_exit();
      return -1;
    }

    // Setup DMA mask
    dma_mask_bit = 32;
    state->device->dma_mask = &state->dma_mask;
    if (dma_set_mask(state->device, DMA_BIT_MASK(dma_mask_bit)) == 0) {
        dma_set_coherent_mask(state->device, DMA_BIT_MASK(dma_mask_bit));
    } else {
        printk(KERN_WARNING "dma_set_mask(DMA_BIT_MASK(%d)) failed\n",
                 dma_mask_bit);
        dma_set_mask(state->device, DMA_BIT_MASK(32));
        dma_set_coherent_mask(state->device, DMA_BIT_MASK(32));
    }

    // Allocate buffer
    state->virt_addr =
      dma_alloc_coherent(state->device, state->alloc_size,
                           &state->phys_addr, GFP_KERNEL);
    if (IS_ERR_OR_NULL(state->virt_addr)) {
      printk(KERN_ERR "dma_alloc_coherent() failed\n");
      state->virt_addr = NULL;
      dmabuffer_module_exit();
      return -1;
    }

    // Initialise char device structure
    cdev_init(&state->cdev, &dmabuffer_fops);
    state->cdev.owner = THIS_MODULE;

    // Create char device
    retval = cdev_add(&state->cdev, state->device_number, 1);
    if (retval != 0) {
      printk(KERN_ERR "dmabuffer: couldn't create devices\n");
      dmabuffer_module_exit();
      return retval;
    }
    state->cdev_valid = 1;
  }
  
  return 0;
}

module_init(dmabuffer_module_init);
module_exit(dmabuffer_module_exit);

MODULE_LICENSE("Dual BSD/GPL");
