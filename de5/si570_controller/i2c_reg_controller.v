// ============================================================================
// Copyright (c) 2013 by Terasic Technologies Inc.
// ============================================================================
//
// Permission:
//
// Terasic grants permission to use and modify this code for use
// in synthesis for all Terasic Development Boards and Altera Development
// Kits made by Terasic. Other use of this code, including the selling
// ,duplication, or modification of any portion is strictly prohibited.
//
// Disclaimer:
//
// This VHDL/Verilog or C/C++ source code is intended as a design reference
// which illustrates how these types of functions can be implemented.
// It is the user's responsibility to verify their design for
// consistency and functionality through the use of formal
// verification methods. Terasic provides no warranty regarding the use
// or functionality of this code.
//
// ============================================================================
//
// Terasic Technologies Inc
//  9F., No.176, Sec.2, Gongdao 5th Rd, East Dist, Hsinchu City, 30070. Taiwan
// HsinChu County, Taiwan
// 302
//
// web: http://www.terasic.com/
// email: support@terasic.com
//
// ============================================================================
// Major Functions: This function is used for configuring si570 register value via 
// i2c_bus_controller .
//
//
// ============================================================================
// Design Description:
//
//
//
//
// ===========================================================================
// Revision History :
// ============================================================================
// Ver :| Author :| Mod. Date :| Changes Made:
// V1.0 :| Johnny Fan :| 11/09/30 :| Initial Version
// ============================================================================


`define REG_NUM 9

module i2c_reg_controller(

iCLK, // system   clock 50mhz 
iRST_n, // system reset 
iENABLE, // i2c reg contorl enable signale , high for enable
iI2C_CONTROLLER_STATE, //  i2c controller  state ,  high for  i2c controller  state not in idel 
iI2C_CONTROLLER_CONFIG_DONE,
oController_Ready,
iFREQ_MODE,
oSLAVE_ADDR, 
oBYTE_ADDR,
oBYTE_DATA,
oWR_CMD, // write or read commnad for  i2c controller , 1 for write command 
oStart,  // i2c controller   start control signal, high for start to send signal 
HS_DIV_reg,
N1_reg,
RFREG_reg,
oSI570_ONE_CLK_CONFIG_DONE,
);

//=============================================================================
// PARAMETER declarations
//=============================================================================
parameter write_cmd = 1'b1;
parameter read_cmd = 1'b0;	

//===========================================================================
// PORT declarations
//===========================================================================
input iCLK;
input iRST_n;
input iENABLE;
input iI2C_CONTROLLER_STATE;
input iI2C_CONTROLLER_CONFIG_DONE;
input [2:0] iFREQ_MODE;
output  [6:0] oSLAVE_ADDR;
output [7:0] oBYTE_ADDR;
output [7:0] oBYTE_DATA ;
output oWR_CMD;
output oStart;
output oSI570_ONE_CLK_CONFIG_DONE;	
output [2:0] HS_DIV_reg;
output [6:0] N1_reg;
output [37:0] RFREG_reg; 
output	oController_Ready;


//=============================================================================
// REG/WIRE declarations
//=============================================================================

wire [2:0] iFREQ_MODE;
reg [2:0] HS_DIV_reg;
reg [6:0] N1_reg;
reg [37:0] RFREG_reg; 

////////////// write data ////
//wire [7:0] regx_data = 8'h01; // RECALL
wire [7:0] reg0_data = 8'h10; // free DCO
wire [7:0] reg1_data = {HS_DIV_reg,N1_reg[6:2]};
wire [7:0] reg2_data = {N1_reg[1:0],RFREG_reg[37:32]};
wire [7:0] reg3_data = RFREG_reg[31:24];
wire [7:0] reg4_data = RFREG_reg[23:16];
wire [7:0] reg5_data = RFREG_reg[15:8];
wire [7:0] reg6_data = RFREG_reg[7:0];
wire [7:0] reg7_data = 8'h00; // unfree DCO
wire [7:0] reg8_data = 8'h40; //New Freq


//////////////  ctrl addr ////
//wire [7:0] byte_addrx = 8'd135;
wire [7:0] byte_addr0 = 8'd137;
wire [7:0] byte_addr1 = 8'd7;
wire [7:0] byte_addr2 = 8'd8;
wire [7:0] byte_addr3 = 8'd9;
wire [7:0] byte_addr4 = 8'd10;
wire [7:0] byte_addr5 = 8'd11;
wire [7:0] byte_addr6 = 8'd12;
wire [7:0] byte_addr7 = 8'd137;
wire [7:0] byte_addr8 = 8'd135;

wire [6:0] slave_addr = 0;

reg [`REG_NUM/2:0] i2c_reg_state;

wire [6:0] oSLAVE_ADDR = i2c_ctrl_data[23:17];
wire [7:0] oBYTE_ADDR = i2c_ctrl_data[16:9];
wire [7:0] oBYTE_DATA = i2c_ctrl_data[8:1];
wire  oWR_CMD = i2c_ctrl_data[0];

wire oStart = access_next_i2c_reg_cmd;
wire i2c_controller_config_done;

reg [23:0] 	i2c_ctrl_data;//  slave_addr(7bit) + byte_addr(8bit) + byte_data(8bit)+ wr_cmd (1bit) = 24bit

wire access_next_i2c_reg_cmd ;
wire access_i2c_reg_start;
wire oSI570_ONE_CLK_CONFIG_DONE;
reg	oController_Ready;
//=============================================================================
// Structural coding
//=============================================================================

//=====================================
//  Write & Read  reg flow control 
//=====================================

always@(iFREQ_MODE or HS_DIV_reg or N1_reg or RFREG_reg)
begin
	case (iFREQ_MODE)
		0:   //100Mhz
			begin
				HS_DIV_reg <= 3'b101;
				N1_reg <= 7'b0000101;
				RFREG_reg <= 38'h2F40135A9;
			end
		1:   //125Mhz
			begin
				HS_DIV_reg <= 3'b111;
				N1_reg <= 7'b0000011;
				RFREG_reg <= 38'h302013B65;
			end
		2:   //156.25Mhz
			begin
				HS_DIV_reg <= 3'b101;
				N1_reg <= 7'b0000011;
				RFREG_reg <= 38'h313814290;
			end			
		3:   //250Mhz
			begin
				HS_DIV_reg <= 3'b111;
				N1_reg <= 7'b0000001;
				RFREG_reg <= 38'h302013B65;
			end
		4:   //312.5Mhz
			begin
				HS_DIV_reg <= 3'b101;
				N1_reg <= 7'b0000001;
				RFREG_reg <= 38'h313814290;
			end	
		5:   //322.265625Mhz
			begin
				HS_DIV_reg <= 3'b000;
				N1_reg <= 7'b000011;
				RFREG_reg <= 38'h2D1E127AF;
			end			
		6:   //644.53125Mhz
			begin
				HS_DIV_reg <= 3'b000;
				N1_reg <= 7'b000001;
				RFREG_reg <= 38'h2D1E127AF;
			end			
		7:   //100Mhz
			begin
				HS_DIV_reg <= 3'b101;
				N1_reg <= 7'b0000101;
				RFREG_reg <= 38'h2F40135A9;
			end			
	endcase
end	
			
//=====================================
//  State control
//=====================================
			
			
always@(posedge iCLK or negedge iRST_n)
	begin
		if (!iRST_n)
			begin
				i2c_reg_state <= 0;
			end
		else
			begin
				if (access_i2c_reg_start)
					i2c_reg_state <= 1'b1;
				else if (i2c_controller_config_done)
					i2c_reg_state <= i2c_reg_state+1;
				else if (i2c_reg_state == (`REG_NUM+1))
					i2c_reg_state <= 0;	
			end
	end

//=====================================
//  i2c bus address & data control 
//=====================================	
always@(i2c_reg_state or i2c_ctrl_data)
begin
	i2c_ctrl_data = 0;
	case (i2c_reg_state)
		0:  i2c_ctrl_data = 0; // don't forget to change REG_NUM value 
		1:  i2c_ctrl_data = {slave_addr,byte_addr0,reg0_data,write_cmd};
		2:  i2c_ctrl_data = {slave_addr,byte_addr1,reg1_data,write_cmd};
		3:  i2c_ctrl_data = {slave_addr,byte_addr2,reg2_data,write_cmd};
		4:  i2c_ctrl_data = {slave_addr,byte_addr3,reg3_data,write_cmd};
		5:  i2c_ctrl_data = {slave_addr,byte_addr4,reg4_data,write_cmd};
		6:  i2c_ctrl_data = {slave_addr,byte_addr5,reg5_data,write_cmd};		
		7:  i2c_ctrl_data = {slave_addr,byte_addr6,reg6_data,write_cmd};
		8:  i2c_ctrl_data = {slave_addr,byte_addr7,reg7_data,write_cmd};
		9:  i2c_ctrl_data = {slave_addr,byte_addr8,reg8_data,write_cmd};	
//	  10:  i2c_ctrl_data = {slave_addr,byte_addr8,reg8_data,write_cmd};	
	endcase	
end 



edge_detector u1(

.iCLK(iCLK),
.iRST_n(iRST_n),
.iIn(iI2C_CONTROLLER_CONFIG_DONE),
.oFallING_EDGE(i2c_controller_config_done),
.oRISING_EDGE()
);


always@(posedge iCLK or negedge iRST_n)
	begin
		if (!iRST_n)
			begin
				oController_Ready <= 1'b1;
			end
		else if (i2c_reg_state == `REG_NUM+1)	
			begin
				oController_Ready <= 1'b1;
			end
		else if (i2c_reg_state >0)
			begin
				oController_Ready <= 1'b0;
			end
	end


assign oSI570_ONE_CLK_CONFIG_DONE = ((i2c_reg_state == `REG_NUM) &&(i2c_controller_config_done)) ? 1'b1 : 1'b0;
assign access_next_i2c_reg_cmd = ((iI2C_CONTROLLER_STATE == 1'b0)&&(i2c_reg_state <= `REG_NUM)&&(i2c_reg_state >0)) ? 1'b1 : 1'b0;
assign access_i2c_reg_start = ((iENABLE == 1'b1)&&(iI2C_CONTROLLER_STATE == 1'b0)) ? 1'b1 : 1'b0;
		
		
		
endmodule
