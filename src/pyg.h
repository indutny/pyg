#ifndef SRC_PYG_H_
#define SRC_PYG_H_

#include "src/common.h"
#include "src/queue.h"

#include "parson.h"

/* Forward declarations */
struct pyg_gen_s;

typedef struct pyg_s pyg_t;
typedef struct pyg_state_s pyg_state_t;
typedef struct pyg_target_s pyg_target_t;

struct pyg_s {
  /* 0 - for root, > 0 for child */
  unsigned int id;
  unsigned int child_count;

  JSON_Value* json;
  JSON_Object* obj;
  char* path;
  char* dir;

  pyg_t* root;
  pyg_t* parent;
  struct {
    pyg_hashmap_t map;
    QUEUE list;
  } children;

  struct {
    pyg_hashmap_t map;
    QUEUE list;
  } target;

  QUEUE member;
};

struct pyg_state_s {
  struct pyg_s* pyg;
  struct pyg_gen_s* gen;
  struct pyg_buf_s* out;
};

enum pyg_target_type_e {
  kPygTargetExecutable,
  kPygTargetStatic,
  kPygTargetShared,

  kPygTargetDefaultType = kPygTargetExecutable
};
typedef enum pyg_target_type_e pyg_target_type_t;

struct pyg_target_s {
  pyg_t* pyg;
  JSON_Object* json;

  const char* name;
  pyg_target_type_t type;

  QUEUE member;
  struct {
    pyg_target_t** list;
    unsigned int count;
  } deps;
};

pyg_error_t pyg_new(const char* path, pyg_t** out);
void pyg_free(pyg_t* pyg);

pyg_error_t pyg_translate(pyg_t* pyg, struct pyg_gen_s* gen, pyg_buf_t* out);

#endif  /* SRC_PYG_H_ */
