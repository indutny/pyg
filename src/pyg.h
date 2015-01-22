#ifndef SRC_PYG_H_
#define SRC_PYG_H_

#include "src/common.h"

#include "parson.h"

/* Forward declarations */
struct pyg_gen_s;

typedef struct pyg_s pyg_t;
typedef struct pyg_target_s pyg_target_t;

struct pyg_s {
  JSON_Object* json;
};

struct pyg_target_s {
  const char* name;
};

pyg_t* pyg_new(JSON_Object* json);
void pyg_free(pyg_t* pyg);

pyg_error_t pyg_translate(pyg_t* pyg, struct pyg_gen_s* gen, pyg_buf_t* buf);

#endif  /* SRC_PYG_H_ */
