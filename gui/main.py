"""Точка входа GUI и скрытого рабочего процесса."""
import sys


def main():
    if len(sys.argv) == 4 and sys.argv[1] == '--worker':
        from jobs import worker_main
        worker_main(sys.argv[2], sys.argv[3])
    else:
        # Настройка DPI относится только к этому процессу, не к системе.
        if sys.platform == 'win32':
            import ctypes
            try:
                ctypes.windll.shcore.SetProcessDpiAwareness(1)
            except (AttributeError, OSError):
                pass
        from window import Application
        app = Application()
        app.mainloop()


if __name__ == '__main__':
    main()
