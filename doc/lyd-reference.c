#include <stdio.h>
#include <string.h>

#define TOP "<a class='jump' href='#top' title='jump to top'>⇡</a>"

int main (int argc, char **argv)
{
  printf ("<html> "
 "<head><title>lyd - language reference </title>"
 "<meta http-equiv='Content-Type' content='text/html; charset=UTF-8'/> "
 "<style type='text/css'>@import url(lyd.css)</style></head>"
 "<body><a name='top'></a>"
 "<center><a href='index.html'><img id='logo' src='lyd.png'/></a></center>"
 "<div class='content'>"
 "<h3>"TOP"<a name='reference' href='#reference'>Language reference</a></h3>\n"
 "<p>This reference documents all the reserved words in the lyd language, these a pre-existing sound sources and filters.</p>"
 "<h3>"TOP"<a name='keywords' href='#keywords'>Arithmetic</a></h3><p>Parantheses can be used to override or document precedence for the in-fix operations +,-,*,/,%% and ^</p\n");
  printf ("<p>");
#define LYD_OP(TOKEN,ENUM,ARG_COUNT,CODE,INIT,FREE,DOC,ARG_DOC) \
  if (!strcmp(TOKEN, "adsr")) printf ("</p><h3>"TOP"<a name='sources' href='#sources'>Sources</a></h3>"\
                                      "<p>Sources are oscillators and other signal generators generate a time/sample dependent output and provides the changing elements of the equation expressed in the lyd language.</p><p>\n");\
  if (!strcmp(TOKEN, "reverb")) printf ("</p><h3>"TOP"<a name='filters' href='#filters'>Filters</a></h3><p>Filters adjust various qualities of a signal, ranging from providing chorus to masking out portions of the signal.</p><p>\n");\
  if (!strcmp(TOKEN, "mix")) printf ("</p><h3>"TOP"<a name='mixers' href='#mixers'>Mixers</a></h3><p>Mixers </p><p>\n");\
  printf ("<a href='#%s'>%s</a>\n", TOKEN, TOKEN);
#include "lyd-ops.inc"
#undef LYD_OP
  printf ("</p>");


#define LYD_OP(TOKEN,ENUM,ARG_COUNT,CODE,INIT,FREE,DOC,ARGS) \
  printf ("<h3>"TOP"<a name='%s' href='#%s'>%s %s</a></h3><p>%s</p>\n", TOKEN, TOKEN, TOKEN, ARGS, DOC);
#include "lyd-ops.inc"
#undef LYD_OP

  printf ("</div> </body> </html>\n");
  return 0;
}
