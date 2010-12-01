#include <stdio.h>
#include <string.h>


int main (int argc, char **argv)
{
  printf ("<html> "
 "<head><title>lyd - language reference </title>"
 "<meta http-equiv='Content-Type' content='text/html; charset=UTF-8'/> "
 "<style type='text/css'>@import url(lyd.css)</style></head>"
 "<body> <center><a href='index.html'><img id='logo' src='lyd.png'/></a></center>"
 "<div class='content'>"
 "<h3><a name='reference' href='#reference'>Reference</a></h3>\n"
 "<p>The core functionality of lyd is a language to express functions"
 "generating sound. A sound is programmed as an equation that is continously"
 "being evaluated for new samples over time where variables in the equation"
 "change over time.</p>"
 "<p>The lyd language is tries to be a simple expression parser for defining and processing audio signals. It is modeled on traditional mathematical in-fix notation with additional functions taking multiple arguments.</p>"
 "<p>Any string encountered that is not one of the recognized keywords is treated as a new variable, at first instantiation a variable can be given an initial value with =  <tt>sin(hz=100.0)</tt>  these variables can later be updated on a sample accurate basis when the expression is executing.</p>"

 "<h3><a name='keywords' href='#keywords'>Arithmetic</a></h3><p>Mathematical functions, paranthesises can be used to override or document precedence for the in-fix operations +,-,*,/,%% and ^</p\n");
  printf ("<p>");
#define LYD_OP(TOKEN,ENUM,ARG_COUNT,CODE,DOC,ARG_DOC) \
  if (!strcmp(TOKEN, "adsr")) printf ("</p><h3><a name='oscillators' href='#oscillators'>Oscillators</a></h3>"\
                                      "<p>Oscillators and other signal generators generate a time/sample dependent output and provides the changing elements of the equation expressed in the lyd language.</p><p>\n");\
  if (!strcmp(TOKEN, "reverb")) printf ("</p><h3><a name='filters' href='#filters'>Filters</a></h3><p>Filters adjust various qualities of a signal, ranging from providing chorus to masking out portions of the signal.</p><p>\n");\
  if (!strcmp(TOKEN, "mix")) printf ("</p><h3><a name='mixers' href='#mixers'>Mixers</a></h3><p>Mixers </p><p>\n");\
  printf ("<a href='#%s'>%s</a>\n", TOKEN, TOKEN);
#include "lyd-ops.inc"
#undef LYD_OP
  printf ("</p>");

#define LYD_OP(TOKEN,ENUM,ARG_COUNT,CODE,DOC,ARGS) \
  printf ("<h3><a name='%s' href='#%s'>%s %s</a></h3><p>%s</p>\n", TOKEN, TOKEN, TOKEN, ARGS, DOC);
#include "lyd-ops.inc"
#undef LYD_OP

  printf ("</div> </body> </html>\n");
  return 0;
}
