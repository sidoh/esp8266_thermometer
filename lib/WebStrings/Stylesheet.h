#include <stddef.h>
#include <pgmspace.h>

#ifndef _STYLESHEET_H
#define _STYLESHEET_H

const char STYLESHEET[] PROGMEM = R"(
.thermometer-section { 
  margin-top: 6em; 
}
.page-header { 
  margin-top: 5em;
}
.page-header h1 { font-size: 4em; }
#banner { border-bottom: 0; }
.category-title {
  text-transform: uppercase;
  border-bottom: 1px solid #ccc;
  font-size: 1.7em;
}
.btn-radio {
  display: block;
  margin-bottom: 4em;
}
)";

#endif