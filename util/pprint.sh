for i in *.h *.c
do
  sed 's/^ [ ]*\/\* /\
    \/\*/' < $i > xxx

 indent  -bad -bap -bbb -bli0 -cdw -cli2 -cbi -saf -nut -sai \
	-saw -di6 -nbc -bfda -bls -lp -ci2 -i2 -nbbo -l65 -fca \
	-sob -cd24 -ts0 -npcs -T TBLDEF -T COLDEF -T PRXPORT xxx
 mv xxx $i
 rm -f xxx~
done

