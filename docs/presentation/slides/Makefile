# Deck for the 2026-08-12 progress presentation.
#
#   make          figures, then the pdf
#   make watch    latexmk -pvc: rebuild the pdf on every save
#   make figs     regenerate figures/ from the benchmark report only
#   make clean    drop build intermediates (keeps slides.pdf)
#
# The pdf is committed, so `make` must be reproducible from a clean checkout:
# figures/ is generated and gitignored, the report it reads is committed.

REPORT := ../../runs/2026-08-11-benchmarks-release.json
FIGS   := figures/speedup.pdf figures/structural.pdf figures/cost.pdf

# Keep matplotlib's cache inside the deck dir; $HOME/.config is read-only here.
export MPLCONFIGDIR := $(CURDIR)/.mplcache

.PHONY: all figs watch clean distclean

all: slides.pdf

$(FIGS): charts.py $(REPORT)
	python3 charts.py

figs: $(FIGS)

slides.pdf: slides.tex $(FIGS)
	latexmk -pdf -halt-on-error -interaction=nonstopmode slides.tex

# Live loop: latexmk watches slides.tex and the figures, and refreshes the
# viewer. Re-run `make figs` in another shell after editing charts.py.
watch: $(FIGS)
	latexmk -pdf -pvc -halt-on-error -interaction=nonstopmode slides.tex

clean:
	latexmk -c slides.tex
	rm -rf .mplcache

distclean: clean
	rm -f slides.pdf
	rm -rf figures
