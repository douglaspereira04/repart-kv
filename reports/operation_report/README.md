# Relatório

## Finalidade

Este diretório traz um relatório mínimo em LaTeX configurado com a classe ABNT (`abntex2`) e um script auxiliar que gera o PDF.

## Dependências

Instale uma distribuição TeX Live com suporte à língua portuguesa e a classe `abntex2`. No Ubuntu/Debian:

```bash
sudo apt-get update
sudo apt-get install -y \
  texlive-latex-recommended \
  texlive-lang-portuguese \
  abntex2 \
  texlive-fonts-recommended \
  texlive-latex-extra
```

O pacote `abntex2` garante que a classe usada no relatório esteja disponível. Ajuste conforme sua distribuição ou use outra forma de instalar LaTeX se preferir.

## Compilar

Execute o script para gerar `report/out/main.pdf`:

```bash
./report/build.sh
```

O script roda `pdflatex` duas vezes e coloca o resultado em `report/out/main.pdf`.
