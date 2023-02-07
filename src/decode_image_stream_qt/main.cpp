#include <QtWidgets/QApplication>
#include <QtCore/QCommandLineParser>

#include "Widget.h"

int main(int argc, char *argv[]) {
    try {
        QApplication app(argc, argv);
        QCommandLineParser parser;
        parser.setApplicationDescription("decode image stream");
        parser.addHelpOption();
        parser.addPositionalArgument("output_file", "output file");
        parser.addPositionalArgument("symbol_type", "symbol type");
        parser.addPositionalArgument("dim", "dim as tile_x_num,tile_y_num,tile_x_size,tile_y_size");
        parser.addPositionalArgument("part_num", "part num");
        parser.addOptions({
            {"mp", "multiprocessing", "number"},
        });
        parser.process(app);
        QStringList args = parser.positionalArguments();
        if (args.size() < 4) {
            parser.showHelp(1);
        }
        std::string output_file = args.at(0).toStdString();
        auto symbol_type = parse_symbol_type(args.at(1).toStdString());
        auto dim = parse_dim(args.at(2).toStdString());
        uint32_t part_num = std::stoul(args.at(3).toStdString());
        int mp = 1;
        if (parser.isSet("mp")) {
            mp = std::stoi(parser.value("mp").toStdString());
        }
        Widget widget(nullptr, output_file, symbol_type, dim, part_num, mp);
        widget.show();
        return app.exec();
    }
    catch (std::exception& e) {
        std::cerr << e.what() << "\n";
        return 1;
    }
}
