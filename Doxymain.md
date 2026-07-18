\mainpage Doxygen output for SoX

In "classes" above you get a list of all types but the best page is:

* [A list of all types and functions](sox__ng_8h.html)

of which you want to use the `sox_*` and `SOX_*` ones,
not the `lsx_*` and `LSX_*` ones which are internal to `libsox`,
unless you're writing a new format or effect handler,
in which case you want to see `src/skelform.c` and `src/skeleff.c`
in the source code.

For examples of using `libsox`, see `src/example?.c`.
