/\*\*

---

- 实验简介
- 实验名称：LVGL综合实验
- 实验平台：慧勤智远 ESP32-P4 开发板
- 实验目的：LVGL综合实验

---

- 硬件资源及引脚分配
- 1 LED
  LED0 - IO14
  LED1 - IO13
- 2 7 寸 WKS MIPI屏
- 3 TOUCH
  SDA - IO17
  SCL - IO16
  INT - IO15

---

- 实验现象
- 1 开机后显示UI界面，点击图标，进入对应的应用程序。
  电脑端： 用 Python 一行命令启动 HTTP 服务供固件下载：

```
python -m http.server 8080
```

然后把 build/comprehensive_routine_70inch_mipilcd.bin 放到该目录下，调用 ota_trigger("http://电脑IP:8080/comprehensive_routine_70inch_mipilcd.bin")

---

- 注意事项
- 1 仅支持WKS 7寸 1024\*600 MIPI屏.

\*/
