all: cask cask.dvi


qcask:	  qcask.c
	  $(GCC) $< -o $@
cask:	cask.o
	$(CC) -o cask cask.o

cask.o:	cask.c
	$(CC) -std=gnu99 -c cask.c

cask.c:	cask.w
	$(CTANGLE) cask.w

cask.dvi:	cask.tex
	$(TEX) cask
	mv cask.dvi cask.tmp.dvi
	dviselect -i cask.tmp.dvi -o front.dvi =15:15
	dviselect -i cask.tmp.dvi -o back.dvi =1:14
	dviconcat -o cask.dvi front.dvi back.dvi

cask.tex:	cask.w chapter-references.w references.tex
	$(CWEAVE) cask.w

clean:
	$(RM) cask.tex cask.o cask.c
