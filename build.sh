#!/bin/sh
# Использование: build.sh <исходный_файл>
# Коды: 1 - аргументы, 2 - нет файла, 3 - нет Output:, 4 - тип файла,
#       5 - mktemp/cd, 6 - ошибка компиляции, 7 - нет результата
# Сигналы: 129 HUP, 130 INT, 131 QUIT, 143 TERM

tmp=""

cleanup() {
    [ -n "$tmp" ] && rm -rf -- "$tmp"
}

trap cleanup EXIT
trap 'exit 129' HUP
trap 'exit 130' INT
trap 'exit 131' QUIT
trap 'exit 143' TERM

die() {
    echo "$2" >&2
    exit "$1"
}

[ $# -eq 1 ] || die 1 "использование: $0 <исходный_файл>"
src=$1
[ -f "$src" ] || die 2 "файл не найден: $src"

srcdir=$(cd "$(dirname "$src")" && pwd)
srcname=$(basename "$src")
srcabs="$srcdir/$srcname"

out=$(grep -E '(/\*|//|%|#)[[:space:]]*Output:' "$srcabs" | head -n 1 \
    | sed 's/.*Output:[[:space:]]*\([^[:space:]]*\).*/\1/')
[ -n "$out" ] || die 3 "нет комментария Output:"
case $out in
    */*|.|..) die 3 "некорректное имя файла: $out" ;;
esac

tmp=$(mktemp -d) || die 5 "не удалось создать временный каталог"
cd "$tmp" || die 5 "не удалось перейти во временный каталог"

case $srcname in
    *.c)
        cc -I"$srcdir" -o "$out" "$srcabs" || die 6 "ошибка компиляции"
        built=$out
        ;;
    *.cpp|*.cc|*.cxx)
        c++ -I"$srcdir" -o "$out" "$srcabs" || die 6 "ошибка компиляции"
        built=$out
        ;;
    *.tex)
        TEXINPUTS="$srcdir:" pdflatex -interaction=nonstopmode -halt-on-error \
            -jobname=result "$srcabs" >tex.log 2>&1 \
            || { cat tex.log >&2; die 6 "ошибка компиляции"; }
        built=result.pdf
        ;;
    *)
        die 4 "неподдерживаемый тип файла"
        ;;
esac

[ -f "$built" ] || die 7 "конечный файл не получен"
cp -- "$built" "$srcdir/$out" || die 7 "не удалось скопировать результат"
