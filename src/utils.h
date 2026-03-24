#ifndef UTILS_H
# define UTILS_H
# include <stdio.h>

# define LOG(message) printf(message"\n")
# define R_LOG(message, rc) printf(message" (0x%x)\n", rc)
# define ALIGN_UP(size, alignment) (((size) + (alignment) - 1) & ~((alignment) - 1))

#endif