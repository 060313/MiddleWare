// agent 子系统冒烟测试
#include <cstdio>

#include "SchedManager.h"

int main() {
    agent::SchedManager mgr;

    std::printf("[agent_test] initial mode = %s\n", common::toString(mgr.mode()));

    common::TaskFeature task;
    task.taskId = 1;
    task.batchSize = 4;
    const common::ActionInfo info = mgr.handleTask(task);
    std::printf("[agent_test] handleTask ok=%d actionIndex=%d\n",
                static_cast<int>(info.ok), info.actionIndex);

    // TODO: 断言模式状态机迁移（auto→static→off）；断言模型缺失时降级为 static
    std::printf("[agent_test] TODO: assertions pending\n");
    return 0;
}
