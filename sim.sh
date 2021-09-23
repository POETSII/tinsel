make clean
bash -c "cd rtl && make sim"
make -C hostlink sim/boardctrld
make -C apps/hello sim all
bash -c "cd rtl && ./sim.sh" & bash -c "cd apps/hello && sleep 5 && echo starting app && ./sim"
