#ifndef _COMMAND_HANDLER_H_
#define _COMMAND_HANDLER_H_

/**
 * @brief 初始化命令处理器
 * @return ESP_OK 成功，其他失败
 */
esp_err_t command_handler_init(void);

/**
 * @brief 执行命令
 * @param command_id 命令ID
 */
void command_handler_execute(int command_id);

/**
 * @brief 反初始化命令处理器
 */
void command_handler_deinit(void);

#endif /* _COMMAND_HANDLER_H_ */