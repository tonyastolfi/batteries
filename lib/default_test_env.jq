# default_test_env - The default set of environment variables that control unit tests
#
# input: null
# env:
#  - project_dir: the top-level git repo directory for the project
#  - build_type: one of "Release", "Debug", "RelWithDebInfo"
#
{
  "GTEST_FILTER": "*",
  "GTEST_REPEAT": "1",
  "GTEST_SHUFFLE": "0",
  "GTEST_RANDOM_SEED": "0",
  "GTEST_TOTAL_SHARDS": "1",
  "GTEST_SHARD_INDEX": "0",
  "GTEST_COLOR": "auto",
  "GTEST_BRIEF": "0",
  "GTEST_PRINT_TIME": "1",
  "GTEST_OUTPUT": ("xml:" + $ENV["project_dir"] + "/build/" + $ENV["build_type"] + "/test_results.xml"),
  "GTEST_BREAK_ON_FAILURE": "1",
  "GTEST_CATCH_EXCEPTIONS": "0",
  "GLOG_logtostderr": "1",
  "GLOG_stderrthreshold": "2",
  "GLOG_minloglevel": "0",
  "GLOG_log_dir": "",
  "GLOG_v": "-1",
  "GLOG_vmodule": ""
}
