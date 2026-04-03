#ifndef MOON_CORE_H
#define MOON_CORE_H

// The MOON Standard Library (Embedded as a raw C-string)
static const char *coreLibrary = "## --- MOON STANDARD LIBRARY ---\n"
                                 "\n"
                                 "let show (stuff):\n"
                                 "    give __show(stuff)\n"
                                 "end\n"
                                 "let ask (prompt: String):\n"
                                 "    give __ask(prompt)\n"
                                 "end\n"
                                 "\n"
                                 "let clock:\n"
                                 "    give __clock()\n"
                                 "end\n"
                                 "\n"
                                 "let floor of (n: Number):\n"
                                 "    give __floor(n)\n"
                                 "end\n";

#endif
