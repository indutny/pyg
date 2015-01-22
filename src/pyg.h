#ifndef SRC_PYG_H_
#define SRC_PYG_H_

#include "parson.h"

typedef struct pyg_s pyg_t;

struct pyg_s {
  JSON_Value* json;
};

pyg_t* pyg_new(JSON_Value* json);
void pyg_free(pyg_t* pyg);

#endif  /* SRC_PYG_H_ */
