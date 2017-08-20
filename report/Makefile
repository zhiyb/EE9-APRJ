SRC	:= $(shell egrep -l '^[^%]*\\begin\{document\}' *.tex)
PDF	= $(SRC:%.tex=%.pdf)
TRG	= $(SRC:%.tex=%.dvi)
BIB	= $(wildcard *.bib)

.PHONY: all
all: $(PDF)

%.pdf: %.tex %.bbl
	pdflatex --shell-escape $<
	pdflatex --shell-escape $<

%.bbl: $(BIB)
	pdflatex --shell-escape $(@:%.bbl=%.tex)
	-bibtex $(@:%.bbl=%.aux)

$(PDF): $(wildcard *.tex)

.PHONY: clean
clean:
	rm -f *.aux *.bbl *.blg *.log $(PDF) $(TRG:%.dvi=%.aux) $(TRG:%.dvi=%.bbl) $(TRG:%.dvi=%.blg) $(TRG:%.dvi=%.log) $(TRG:%.dvi=%.out) $(TRG:%.dvi=%.idx) $(TRG:%.dvi=%.ilg) $(TRG:%.dvi=%.ind) $(TRG:%.dvi=%.toc) $(TRG:%.dvi=%.d) $(TRG:%.dvi=%.lof) $(TRG:%.dvi=%.lol) $(TRG:%.dvi=%.lot)

.PHONY: evince
evince: $(PDF)
	evince $(PDF)

.PHONY: acrord
acrord: $(PDF)
	$(eval TMPPDF = LaTeX-$(shell sha1sum $< | awk '{print $$1}').pdf)
	$(MAKE) $(WINTMP)/$(TMPPDF)
	cd $(WINTMP); "$(ACRORD)" $(TMPPDF); true

$(WINTMP)/%.pdf: $(PDF)
	cp -a $< $@
