# 基于 C++ 的限价订单簿与订单撮合引擎

C++ Limit Order Book and Matching Engine

基于 C++17、STL 和 CMake 的单标的限价撮合项目。分八阶段开发，当前已实现订单模型、订单簿索引、提交管理及价格时间优先撮合核心，独立成交管理、业务撤单和市场深度查询已实现。不连接交易所，不提供真实下单。

## 当前状态

第一至第七阶段均已验收；第七阶段已于2026-11-13由负责人验收通过，第八阶段未开始。验证结果见[第一阶段验收报告](docs/第一阶段_总体架构与订单模型验收.md)及[第二阶段验收报告](docs/第二阶段_订单簿与订单索引验收.md)。测试现包含原有六组CTest契约程序及七个GoogleTest回归案例。

## Linux 构建

需要支持 C++17 的 GCC/Clang 和 CMake 3.20+。

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel 2
ctest --test-dir build --output-on-failure
./build/matching_engine
```

## 订单簿结构接口

`OrderBook`以只读引用关联调用方持有的`OrderStore`，支持`insert`、结构`erase`、最优订单/价格、档位快照和一致性检查。订单表必须比订单簿更晚销毁，活动订单不得外部删除或替换；订单簿禁止复制移动。`erase`不修改订单状态，不代表业务撤单。成功入簿序号必须严格递增；数量溢出明确拒绝，不回绕。第三阶段已实现OrderManager提交管理；第四阶段已实现价格时间优先撮合、部分/完全成交状态流转和基础成交记录。

第二阶段本机Windows Debug编译通过，CTest为2个契约程序通过；第三阶段本机CTest为3/3程序通过。第二、三阶段尚未运行Linux验证；第一阶段Ubuntu结果不替代后续阶段证据。

## 撮合规则（第四阶段已实现）

买盘价格降序，卖盘价格升序；相同价格 FIFO。新买单价格 >= 最低卖价、新卖单价格 <= 最高买价时可撮合，成交价采用订单簿中已有的被动方价格。每次对手方匹配生成独立成交；数量取双方剩余数量最小值。剩余有效数量入簿，已成交或已撤销订单不能再次成交。

价格用 int64 整数 tick，不用浮点直接比较。第一版单标的、单线程；不含市价单、改单、费用、持仓账户和网络交易接口。

## 文档

- [需求说明书](docs/需求说明书.md)
- [总体设计](docs/总体设计.md)
- [开发计划](docs/开发计划.md)
- [第一阶段验收](docs/第一阶段_总体架构与订单模型验收.md)
- [第二阶段验收](docs/第二阶段_订单簿与订单索引验收.md)

学习与技术问答保留在阶段文档；后续阶段须经负责人验收后推进。仓库不保存构建产物及性能结果数据。

## 第三阶段：提交管理

MatchingEngine::submitOrder接收OrderRequest；OrderManager拥有历史、订单簿、ID账本与递增序号。合法买卖单入簿，非法请求返回明确REJECTED结果。非零ID含业务拒单均不可复用；拒单不消耗序号。第三阶段当时未撮合；当前MatchingEngine入口已启用第四阶段撮合，OrderManager::submit仍为低层结构提交接口。submission为首次提交结果，实时订单查find。

验证：本机C++17 Debug构建成功，CTest 3/3程序通过；当前阶段尚无Linux验证。详见[第三阶段验收报告](docs/第三阶段_订单提交与管理验收.md)。

## 第四阶段：价格时间优先撮合

第四阶段已于2026-11-13由负责人验收通过。第一至第四阶段均已验收。正式submitOrder按价格优先、同价FIFO匹配，使用被动方价格；更新双方剩余量和状态，未成交残量入簿，终态清活动索引但保留历史。trades()只读查询基础逐笔成交。示例卖100tick/100、买101tick/40，成交100tick/40，卖方剩60。

本机C++17 Debug构建15步成功，CTest 4/4程序通过；本阶段未运行Linux CI。第五阶段现已实现独立TradeManager，结果见下文；此处4/4为第四阶段历史测试记录。第四阶段代码、测试与验收文档随本次阶段发布提交。详见[第四阶段验收报告](docs/第四阶段_价格时间优先撮合验收.md)。

## 第五阶段：成交管理与状态边界

TradeManager统一拥有成交历史、分配成交ID/序号并预留批次容量，保持被动价和FIFO规则。trades()保留只读vector接口，trade_manager()提供数量及最后序号查询。序号不足整笔返回TRADE_SEQUENCE_EXHAUSTED，不修改被动订单或发布部分成交；成交容量按几何增长。不要跨后续提交保存vector元素地址。

本机C++17 Debug构建18步成功，CTest 5/5程序通过，4.63秒，覆盖双向部分/完全成交、连续成交、数量守恒、终态清理、序号上限及400笔历史扩容。第五阶段已于2026-11-13由负责人验收通过，尚无当前Linux、GoogleTest、故障注入或性能证据。详见[第五阶段验收报告](docs/第五阶段_部分完全成交与成交记录验收.md)。

## 第六阶段：撤单与市场深度

`MatchingEngine::cancelOrder(id)`返回CancelResult，区分NOT_FOUND、REJECTED_ORDER、ALREADY_FILLED、ALREADY_CANCELED；成功为NONE。只允许NEW/PARTIALLY_FILLED撤销，移除活动索引与空档后转CANCELED，保留历史和未成交余额。剩余量在CANCELED状态表示已撤销的未执行量，不再是活动委托；撤单不生成成交、不占用接收或成交序号。首次提交结果不变，当前状态查find。

`marketDepth()`返回独立DepthSnapshot，含best_bid/best_ask、排序后的bids/asks及活动订单数；空侧最优价为nullopt。价档数量为剩余量汇总，快照不受后续交易影响；它不是实时订阅。当前汇总扫描活动订单，撤单平均O(log L)，深度平均O(N+L)，哈希最坏可退化。

本机C++17 Debug构建21步成功，CTest 6/6程序通过，5.22秒；没有当前Linux、GoogleTest、Sanitizer或性能基准证据。详见[第六阶段验收报告](docs/第六阶段_撤单与市场深度验收.md)。第六阶段已验收，本次将代码、测试与关联文档提交并推送GitHub。

## 第七阶段：单元测试与回归验证

GoogleTest v1.15.2由CMake FetchContent获取，默认开启MATCHING_BUILD_GTEST；首次配置需要访问依赖源。已备好源码时，可传入`-DFETCHCONTENT_SOURCE_DIR_GOOGLETEST=<源码目录>`。离线且没有源码时，可配置`-DMATCHING_BUILD_GTEST=OFF`，仅运行原有六组契约测试；这不等于运行完整第七阶段矩阵。

新增测试覆盖双向被动价/FIFO、跨档及残量、终态/快照、非法请求、成交序号整批拒绝及价档溢出。独立vector参考模型使用三个固定种子，合计2400次混合操作，逐步核对成交、状态、数量守恒和市场深度。

2026-11-13本机Windows C++17 Debug构建31步成功，CTest **13/13通过，6.14秒**（7个GoogleTest案例+6组契约程序）。样例运行成功。GoogleTest配置缺C编译器问题已通过补C包装器解决；ABI探测/目标架构警告仍存在。本轮未运行Linux CI、Sanitizer、内存故障注入或性能基准。此前章节的测试计数与未实现范围均按当时阶段理解。

详见[第七阶段验收报告](docs/第七阶段_单元测试与回归验证验收.md)。第七阶段已验收，本次将测试、CMake及关联文档提交并推送GitHub；第八阶段未开始。
