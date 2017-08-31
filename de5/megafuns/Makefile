.PHONY: all
all: phy phy_reconfig fp

phy: phy.v
	mkdir -p phy
	cp -a phy.v phy
	cd phy && qmegawiz -silent phy.v

phy_reconfig: phy_reconfig.v
	mkdir -p phy_reconfig
	cp -a phy_reconfig.v phy_reconfig
	cd phy_reconfig && qmegawiz -silent phy_reconfig.v

fp:
	mkdir -p fp
	cp -a AlteraFPAddSub.v fp
	cp -a AlteraFPMult.v fp
	cp -a AlteraFPDiv.v fp
	cp -a AlteraFPCompare.v fp
	cp -a AlteraFPToInt.v fp
	cp -a AlteraFPFromInt.v fp
	cp -a AlteraFPSqrt.v fp
	cd fp && qmegawiz -silent AlteraFPAddSub.v
	cd fp && qmegawiz -silent AlteraFPMult.v
	cd fp && qmegawiz -silent AlteraFPDiv.v
	cd fp && qmegawiz -silent AlteraFPCompare.v
	cd fp && qmegawiz -silent AlteraFPToInt.v
	cd fp && qmegawiz -silent AlteraFPFromInt.v
	cd fp && qmegawiz -silent AlteraFPSqrt.v

.PHONY: clean
clean:
	rm -rf phy phy_reconfig fp
