# line
a line util to cleanly and sanely extract line(s)

## Use cases
 - Printing a specific line from an input
 - Printing a range of lines
 - Any combination of the first two

## Usage

```
line - a line util to cleanly and sanely extract line(s)
Usage:
    line <OPTIONS> MARKER...
Options:
    -[-l]ine MARKER - line marker explicitly
       range: <#>-<#>        - 4-10
       list: <#>,<#>,<#>,... - 1,2,3
       single: <#>           - 8
    -[-f]ile FILE - input file. defaults to "-" for stdin
    -[-h]elp - this message
Examples:
   line 1 2,5,7 440-1000
   line -l 500-1000 -f /path/to/file
```
