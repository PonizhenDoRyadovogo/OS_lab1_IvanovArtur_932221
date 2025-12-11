#include <linux/kernel.h> // Основной заголовок ядра Linux
#include <linux/module.h> // Заголовок для создания модулей ядра
#include <linux/init.h> // Заголовок для функций инициализации и завершения модуля
#include <linux/printk.h> // Заголовок для вывода сообщений в лог ядра
#include <linux/proc_fs.h> // Заголовок для работы с файловой системой /proc
#include <linux/uaccess.h> // Заголовок для передачи данных между пространством ядра и пользовательским пространством
#include <linux/version.h> // Заголовок для проверки версии ядра
#include <linux/string.h>


MODULE_DESCRIPTION("Tomsk State University Kernel Module");
MODULE_AUTHOR("mktrhr");

#define PROCFS_NAME "tsulab" // Имя файла в файловой системе /proc
static struct proc_dir_entry* proc_file = NULL; // Указатель на созданный файл в /proc

// Чтение файла в /proc
static ssize_t proc_read(struct file* file, char __user* buffer, size_t len, loff_t* offset)
{
    const char* message = "Hello from Tomsk State University!\n"; // Сообщение, возвращаемое пользователю
    size_t msg_len = strlen(message); // Длина сообщения

    if (*offset >= msg_len) { // Проверка, достигнут ли конец сообщения
        return 0; // Завершаем чтение
    }

    if (copy_to_user(buffer, message, msg_len)) { // Передача данных пользователю
        pr_err("Failed to copy data to user space\n"); // Лог ошибки, если передача данных не удалась
        return -EFAULT; // Возврат кода ошибки
    }

    *offset += msg_len; // Обновляем смещение для поддержки повторного чтения
    return msg_len; // Возвращаем длину сообщения
}

static const struct proc_ops proc_file_ops = {
    .proc_read = proc_read, // Указатель на функцию чтения для современных ядер
};


// Инициализация модуля
static int __init tsu_init(void)
{
    pr_info("Welcome to the Tomsk State University\n"); // Лог сообщения при загрузке модуля

    proc_file = proc_create(PROCFS_NAME, 0644, NULL, &proc_file_ops); // Создание файла в /proc с указанными правами
    if (!proc_file) { // Проверка успешности создания файла
        pr_err("Failed to create /proc/%s\n", PROCFS_NAME); // Лог ошибки при неудачном создании файла
        return -ENOMEM; // Возврат ошибки памяти
    }

    pr_info("/proc/%s created\n", PROCFS_NAME); // Лог успешного создания файла
    return 0; // Успешное завершение функции инициализации
}

// Завершение работы модуля
static void __exit tsu_exit(void)
{
    proc_remove(proc_file); // Удаление файла из /proc
    pr_info("/proc/%s removed\n", PROCFS_NAME); // Лог успешного удаления файла
    pr_info("Tomsk State University forever!\n"); // Лог сообщения при выгрузке модуля
}

module_init(tsu_init); // Указание функции инициализации модуля
module_exit(tsu_exit); // Указание функции завершения работы модуля
MODULE_LICENSE("GPL"); // Лицензия GPL