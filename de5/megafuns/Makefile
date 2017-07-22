.PHONY: all
all: phy phy_reconfig

phy: phy.v
	mkdir -p phy
	cp -a phy.v phy
	cd phy && qmegawiz -silent phy.v

phy_reconfig: phy_reconfig.v
	mkdir -p phy_reconfig
	cp -a phy_reconfig.v phy_reconfig
	cd phy_reconfig && qmegawiz -silent phy_reconfig.v

.PHONY: clean
clean:
	rm -rf phy phy_reconfig
