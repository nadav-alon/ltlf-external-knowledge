# Build the summary paper: `make` for the PDF, `make clean` to drop the aux files.
main.pdf: main.tex macros.tex refs.bib $(wildcard sections/*.tex)
	latexmk -pdf -interaction=nonstopmode -halt-on-error main.tex

clean:
	latexmk -C

.PHONY: clean
