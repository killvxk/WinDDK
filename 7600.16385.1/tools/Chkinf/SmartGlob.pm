package SmartGlob;

        use strict;

        #-------------------------------------------------------------------------------------------------
        #  Accepts an array of file spec's & returns an array of every file that list expands to.
        #  Directories in the argument list are interpreted as Dirname\*.*
        #  Empty filenames are interpreted as .\*.*
        #
        # Handles full, partial, and missing paths, as well as wildcards
        #   (* and ?) in the filename (but not for autogenerated short filenames)
        #     x,   .\x,   ..\x,   \x,   z\y\x,   \z\y\x, etc
        #   a:x, a:.\x, a:..\x, a:\x, a:z\y\x, a:\z\y\x, etc
        #    *, *.*, *.INF, ???.INF, etc (for any x above)
        #   
        # @array SmartGlob(@FileSpecs)
        sub SmartGlob {
            my (@FileSpecs) = @_;
            my ($filespec, $dir, $filepattern, @DirectoryFiles,
                @MatchedFiles, $file, @AllFiles);

            @AllFiles = ();
            
            foreach $filespec (@FileSpecs) {

                if (!defined($filespec) or $filespec =~ /^$/) {
                    $filespec = ".";
                }

                #   is it a directory name ?
                if ( opendir(LOCALDIR,  $filespec) ) {
                    $dir = $filespec . "\\";
                    $filepattern = "*.*";
                } else {
                    # extract the directory path (which includes the trailing : or \)
                    # find the last \ (or : if no slash)
                    if ($filespec !~ /(.*)([:\\])(.*)/) {
                        $dir         = ".\\";
                        $filepattern = $filespec;
                    } else {
                        $dir         = $1 . $2;
                        $filepattern = $3;
                    }

                    next if (!opendir(LOCALDIR,  $dir));
                }

                # Assume tilde (~) in filename means this is a autogenerated short filename
                #   (8.3 name generated from a long file name) and thus we can't
                #   (successfully) pattern match it because 'opendir' only sees long file
                #   names (thus these short names will never be enumerated).  We also don't ensure
                #   it has an .INF extension (I don't know why, since a tilde will be in the
                #   extension only if the extension is more than 3 chars, which "INF" clearly
                #   isn't.)

                if ($filepattern =~ /\~/) {
                    push(@AllFiles, $dir . $filepattern);
                    next;
                }

                @MatchedFiles = ();

                # Turn the pattern into a regex
                $filepattern = quotemeta($filepattern);
                $filepattern =~ s/\\\*/.*/g;
                $filepattern =~ s/\\\?/./g;

                @DirectoryFiles = readdir LOCALDIR;

                foreach $file (sort @DirectoryFiles) {

                    # Skip '.', '..', and any directories
                    next if (($file =~ /^\.$/) or ($file =~ /^\.\.$/) or (-d "$dir$file"));

                    #! Uncomment to Validate Files as Being 8.3 Compliant!
                    # if ($file !~ /^(.{1,8}\..{0,3})$/) {
                    #     print ("Skipping \"$file\": Not a valid 8.3 name!\n") if ($file =~ /\.inf$/);
                    #     next;
                    # }

                    $file .= "." if ($file !~ /\./); # force empty extension (for *.* pattern match ???)

                    push(@MatchedFiles, $dir . $file)
                        if (($file =~ /^$filepattern$/i) &&
                            (! -d "$dir$file") &&
                            ($file =~ /\.inf$/i));
                }
                closedir LOCALDIR;

                # Concatenate the results
                @AllFiles = (@AllFiles, @MatchedFiles);
            }
            my($i);

            # Remove Duplicates!
            @AllFiles = sort(@AllFiles);
            for ($i=1; $i <= $#AllFiles; $i++) {

                if ($AllFiles[$i] eq $AllFiles[$i-1]) {
                    splice(@AllFiles, ($i), 1);
                    $i--; # Shortened the array by one, so decrement
                }
            }

            return(@AllFiles);
        }
1; # Return good status
