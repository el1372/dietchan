[ $# -eq 2 ] || exit 2
< locales perl -F'\t' -lne '
    print("s/\"$F['"$1"']\"/\"$F['"$2"']\"/g");
    ' > change.sed
for f in $@; do
    sed -f change.sed -i "$f"
done
