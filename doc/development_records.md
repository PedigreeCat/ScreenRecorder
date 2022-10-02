# 日志

使用开源库[EasyLogger](https://github.com/armink/EasyLogger)

## 使用方法记录

### 导入项目

1. 下载源码；
2. 将`\easylogger\`（里面包含`inc`、`src`及`port`的那个）文件夹拷贝到项目中；
3. 添加`\easylogger\src\elog.c`、`\easylogger\src\elog_utils.c`及`\easylogger\port\elog_port.c`这些文件到项目的编译路径中（elog_async.c 及 elog_buf.c 视情况选择性添加）；
4. 添加`\easylogger\inc\`文件夹到编译的头文件目录列表中。

由于需要使用写文件功能，实际引入文如下

```
header files:
	- elog.h
	- elog_cfg.h
	- elog_file.h
	- elog_file_cfg.h
	
source files:
	- elog.c
	- elog_utils.c
	- elog_port.c
	- elog_file.c
	- elog_file_port.c
```

> 需要使用对应操作系统demo目录下的elog_port.c替换原来的，因为一些获取系统信息的方法的实现在这些文件中。

### 输出模式开关

将`elog_cfg.h`中关闭颜色、异步输出、缓冲输出，打开文件输出。

```c
/* enable buffered output mode */
#define ELOG_BUF_OUTPUT_ENABLE
/* buffer size for buffered output mode */
#define ELOG_BUF_OUTPUT_BUF_SIZE                 (ELOG_LINE_BUF_SIZE * 10)

#undef ELOG_COLOR_ENABLE
#undef ELOG_ASYNC_OUTPUT_ENABLE
#undef ELOG_BUF_OUTPUT_ENABLE
#define ELOG_FILE_ENABLE

#endif /* _ELOG_CFG_H_ */
```

### 设置日志文件名、切片大小和切片数

将`elog_file_cfg.h`中的宏定义填写好

```c
#ifndef _ELOG_FILE_CFG_H_
#define _ELOG_FILE_CFG_H_

/* EasyLogger file log plugin's using file name */
#define ELOG_FILE_NAME                 "elog_file.log"/* @note you must define it for a value */

/* EasyLogger file log plugin's using file max size */
#define ELOG_FILE_MAX_SIZE             (2 * 1024 * 1024)/* @note you must define it for a value */

/* EasyLogger file log plugin's using max rotate file count */
#define ELOG_FILE_MAX_ROTATE           (10)/* @note you must define it for a value */

#endif /* _ELOG_FILE_CFG_H_ */
```

### 启动模块

添加一个按钮`IDC_BUTTON1`，在按钮的点击函数中添加对日志的测试

```c++
// 引入头文件
#include "elog.h"

void CscreenrecoderDlg::OnBnClickedButton1()
{
	// TODO: 在此添加控件通知处理程序代码
	elog_init();
	elog_set_filter_lvl(ELOG_LVL_DEBUG);
	elog_set_fmt(ELOG_LVL_INFO, ELOG_FMT_ALL & ~ELOG_FMT_TAG & ~ELOG_FMT_P_INFO);
	elog_start();
	for (int i = 0; i < 1024; i++)
		for (int j = 0; j < 512; j++)
			log_i("test: %d.", j);
}
```

## 将日志输出到对话框