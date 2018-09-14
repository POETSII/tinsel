.PHONY: all
all: phy4 phy_reconfig4

phy4: ../../megafuns/phy4.v
	mkdir -p phy4
	cp -a ../../megafuns/phy4.v phy4
	cd phy4 && qmegawiz -silent phy4.v

phy_reconfig4: ../../megafuns/phy_reconfig4.v
	mkdir -p phy_reconfig4
	cp -a ../../megafuns/phy_reconfig4.v phy_reconfig4
	cd phy_reconfig4 && qmegawiz -silent phy_reconfig4.v

.PHONY: clean
clean:
	rm -rf phy4 phy_reconfig4
