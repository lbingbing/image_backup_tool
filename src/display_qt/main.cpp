#include <QtWidgets/QApplication>

#include "Widget.h"

int main(int argc, char *argv[]) {
    try {
        QApplication app(argc, argv);
        Widget widget(nullptr);
        widget.show();
        return app.exec();
    }
    catch (std::exception& e) {
        std::cerr << e.what() << "\n";
        return 1;
    }
}
