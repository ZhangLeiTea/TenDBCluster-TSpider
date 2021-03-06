# TenDB存储层高可用
TenDB存储层的高可用，包括故障的快速发现，故障后的快速切换

## 故障发现
存储层的故障探测方式与TSpider基本相同，可参见[TSpider故障发现](TSpider-failover.md#/jump1)

## 故障切换

### 切换前置要求
在部署集群时，后端每个TenDB要求采用多副本部署，可以根据实际资源情况，采用一主一从、一主多从，已经多主的MGR等方案部署。这样当主库异常时，能快速切换到数据一致性的从库，有效缩减故障时间

### 选择从库
在检测到主库异常后，需要及时的将服务从主库切到从库。  
在选取从库时，无论是一主一从，还是一主多从架构，我们选取的从库要满足如下要求
- 数据一致性

```
从库的数据需要与主库一致。
目前一般推荐的做法是通过数据校验工具，在主库生成数据校验的SQL(会对当前数据做checksum值计算)，主库执行的校验sql同步到从库执行时，会对从库的数据也进行checksum值计算，通过比对主从的checksum值，来判断是否有数据不一致的情况存在
MySQL本身的checksum语法也可以来数据校验，不过在实际校验时，要考虑对已有服务的影响，校验耗时等。  
一致性检查上，无法满足百分百的数据准确性，更多是对故障前已经校验的数据结果进行可靠性检查。当然，在一些严格场景，我们也可以依靠MGR，半同步等解决方案在同步时候就保证数据的一致性
```
- 主从延时

```
在异步复制等场景，因为网络，非并行复制等原因，从库是可能存在延时的，这样在故障时间点，从库的数据执行上可能落后于主库，我们也需要在切换时发现此问题
IO线程延时：逻辑复制下，我们知道MySQL是通过IO线程拉取binlog重放来同步数据的，如果IO线程因为一些不可预知的原因卡住，仅仅通过show slave status等命令来查看同步延时是不准确的，此时show命令查看的结果，只能验证SQL线程在执行上没有落后（本地拉取的binlog都执行完成）。我们可以考虑在主库上周期性的执行一条带当前时间的SQL语句，依赖同步机制，在从库上检测此语句的执行执行从而判断IO线程是否存在落后
SQL线程延时：正常同步场景下，有时SQL线程存在一定的延时是正常的，当主库故障后，此时服务已经存在影响了，不再接收新的数据写入，因此从库是能够继续执行本地已经拉取的binlog，跟进完成的。因此在判断落后时，我们可以适当的采取循环等待、检测的原则，直到slave跟进完成；当然考虑到切换的实时性，我们可以通过调整循环等待时间阀值来决策是否满足切换，有时系统不满足切换时及时抛出异常告警，比长时间的决策等待更好
```

### 故障切换
切换前，确保选取的从库上权限是正常的(否则切换后会因为权限异常而请求失败)，具体权限可以参考[配置TenDB权限](manual-install.md/#jump3)

- 修改集群路由
```sql
update mysql.servers set Host='$slave_ip', Port='$slave_port' where Host='$master_ip' and Port='$master_port';
```

- 刷新路由
```sql
#采用强制刷新方式
tdbctl flush routing force;
```
故障场景下，一些异常的tcp连接在正常销毁前，有时会影响切换的实时性，因此Tdbctl提供了一个强制刷新路由的功能，强制刷新可以有效避免这些异常连接的干扰


### 故障恢复
在故障切换成功后， 此时的TenDB可能是单点，我们要及时恢复新的主从环境，让集群正常运转
