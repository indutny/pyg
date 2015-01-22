#ifndef SRC_PYG_H_
#define SRC_PYG_H_

#include "src/common.h"

#include "parson.h"

/* Forward declarations */
struct pyg_gen_s;

typedef struct pyg_s pyg_t;

struct pyg_s {
  JSON_Value* json;
};

pyg_t* pyg_new(JSON_Value* json);
void pyg_free(pyg_t* pyg);

pyg_error_t pyg_translate(pyg_t* pyg, struct pyg_gen_s* gen, pyg_buf_t* buf);

#endif  /* SRC_PYG_H_ */
