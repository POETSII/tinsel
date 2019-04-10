all: tile.png fpga.png triplet.png box.png custom.png

%.png: %.tex
	pdflatex -shell-escape $<
	pdftoppm $(basename $<).pdf $(basename $<) -png -singlefile -r 125

.PHONY: tidy
tidy:
	rm -f *.log *.md5 *.dpth *.auxlock *.aux *.pdf

.PHONY: clean
clean: tidy
	rm -f *.png
