#ifndef SRC_JSON_H_
#define SRC_JSON_H_

#include "src/common.h"

#include "parson.h"

/* JSON helpers */
enum pyg_merge_mode_e {
  kPygMergeStrict,
  kPygMergeAuto,
  kPygMergeReplace,
  kPygMergeCond,
  kPygMergePrepend,
  kPygMergeExclude
};
typedef enum pyg_merge_mode_e pyg_merge_mode_t;

pyg_error_t pyg_iter_array(JSON_Array* arr,
                           const char* label,
                           pyg_iter_array_get_cb get,
                           pyg_iter_array_cb cb,
                           void* arg);
pyg_error_t pyg_merge_json(JSON_Value* to,
                           JSON_Value* from,
                           pyg_merge_mode_t mode);
pyg_error_t pyg_merge_json_obj(JSON_Object* to,
                               JSON_Object* from,
                               pyg_merge_mode_t mode);
pyg_error_t pyg_clone_json(JSON_Value* value,
                           pyg_merge_mode_t mode,
                           JSON_Value** out);

#endif  /* SRC_JSON_H_ */
