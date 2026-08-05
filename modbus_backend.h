#ifndef MODBUS_BACKEND_H
#define MODBUS_BACKEND_H

/**
 * @file modbus_backend.h
 * @brief 静态链接版 Modbus 后端 C ABI（与 180 动态库符号同名，便于对照）。
 *
 * 本工程将 modbus_backend_c.cpp + libmodbus.a 直接链进可执行文件，
 * 主程序直接调用下列函数，不再使用 QLibrary。
 */

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void *modbus_backend_create(void);
void modbus_backend_destroy(void *handle);

int modbus_backend_connect(void *handle, const char *host, int port, int slave_id);
void modbus_backend_disconnect(void *handle);
int modbus_backend_is_connected(void *handle);

int modbus_backend_read_holding_registers(void *handle, int start_addr, int count,
                                          uint16_t *out_values, int out_capacity);
int modbus_backend_read_input_registers(void *handle, int start_addr, int count,
                                        uint16_t *out_values, int out_capacity);

int modbus_backend_write_single_register(void *handle, int addr, uint16_t value);
int modbus_backend_write_multiple_registers(void *handle, int start_addr,
                                            const uint16_t *values, int count);

#ifdef __cplusplus
}
#endif

#endif // MODBUS_BACKEND_H
