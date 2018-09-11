.PHONY: all
all: phy8 phy_reconfig8 phy4 phy_reconfig4 fp

phy8: phy8.v
	mkdir -p phy8
	cp -a phy8.v phy8
	cd phy8 && qmegawiz -silent phy8.v

phy_reconfig8: phy_reconfig8.v
	mkdir -p phy_reconfig8
	cp -a phy_reconfig8.v phy_reconfig8
	cd phy_reconfig8 && qmegawiz -silent phy_reconfig8.v

phy4: phy4.v
	mkdir -p phy4
	cp -a phy4.v phy4
	cd phy4 && qmegawiz -silent phy4.v

phy_reconfig4: phy_reconfig4.v
	mkdir -p phy_reconfig4
	cp -a phy_reconfig4.v phy_reconfig4
	cd phy_reconfig4 && qmegawiz -silent phy_reconfig4.v

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
	rm -rf phy4 phy_reconfig4 phy8 phy_reconfig8 fp
