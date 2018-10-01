cat $@ | perl -e '
    my %lines = ();
    my @values = map /_\("([^"]*)"\)/g, <>;
    foreach(@values){
        $lines{$_} = 1;
    }
    for(keys %lines){
        print($_, "\n")
    }' 
