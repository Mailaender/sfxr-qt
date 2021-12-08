#include "Generator.h"
#include "Result.h"
#include "Sound.h"
#include "SoundIO.h"
#include "SoundListModel.h"
#include "SoundPlayer.h"
#include "WavSaver.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QDebug>
#include <QDir>
#include <QIcon>
#include <QQmlApplicationEngine>

#include <optional>

using std::optional;

static QIcon createIcon() {
    QIcon icon;
    for (int size : {16, 32, 48}) {
        icon.addFile(QString(":/icons/%1-apps-sfxr-qt.png").arg(size));
    }
    return icon;
}

struct Arguments {
    QUrl url;
    bool export_ = false;
    QUrl outputUrl;
    std::optional<int> outputBits;
    std::optional<int> outputFrequency;

    static optional<Arguments> parse(const QCommandLineParser& parser) {
        Arguments instance;
        auto args = parser.positionalArguments();
        if (args.isEmpty()) {
            return {};
        }

        instance.url =
            QUrl::fromUserInput(args.first(), QDir::currentPath(), QUrl::AssumeLocalFile);

        instance.export_ = parser.isSet("export");
        if (!instance.export_) {
            return instance;
        }

        instance.outputUrl =
            QUrl::fromUserInput(parser.value("output"), QDir::currentPath(), QUrl::AssumeLocalFile);
        if (instance.outputUrl.isEmpty()) {
            auto path = instance.url.path().section('.', 0, -2) + ".wav";
            instance.outputUrl = QUrl::fromLocalFile(path);
        }

        int outputBits = parser.value("bits").toInt();
        if (outputBits > 0) {
            instance.outputBits = outputBits;
        }

        int outputFrequency = parser.value("samplerate").toInt();
        if (outputFrequency > 0) {
            instance.outputFrequency = outputFrequency;
        }

        return instance;
    }
};

static void registerQmlTypes() {
    qmlRegisterType<Sound>("sfxr", 1, 0, "Sound");
    qmlRegisterType<SoundPlayer>("sfxr", 1, 0, "SoundPlayer");
    qmlRegisterType<Generator>("sfxr", 1, 0, "Generator");
    qmlRegisterType<SoundListModel>("sfxr", 1, 0, "SoundListModel");
    qmlRegisterType<WavSaver>("sfxr", 1, 0, "WavSaver");
    WaveForm::registerType();
    Result::registerType();
}

static void setupCommandLineParser(QCommandLineParser* parser) {
    parser->addHelpOption();
    parser->addPositionalArgument("sound_file", QApplication::translate("main", "File to load."));

    parser->addOption(
        {"export",
         QApplication::translate("main", "Create a wav file from the given SFXR file and exit.")});
    parser->addOption(
        {{"o", "output"},
         QApplication::translate("main", "Set the file to export to if --export is given."),
         "file"});
    parser->addOption(
        {{"b", "bits"},
         QApplication::translate("main", "Set the bits per sample of the exported wav."),
         "8 or 16"});
    parser->addOption(
        {{"s", "samplerate"},
         QApplication::translate("main", "Set samplerate in hertz of the exported wav."),
         "22050 or 44100"});
}

static int exportSound(const Arguments& args) {
    Sound sound;
    if (auto res = SoundIO::load(&sound, args.url); !res) {
        qCritical("%s", qUtf8Printable(res.message()));
        return 1;
    }

    WavSaver saver;
    if (args.outputBits.has_value()) {
        saver.setBits(args.outputBits.value());
    }

    if (args.outputFrequency.has_value()) {
        saver.setFrequency(args.outputFrequency.value());
    }

    if (!saver.save(&sound, args.outputUrl)) {
        qCritical("Could not save sound to %s.", qUtf8Printable(args.outputUrl.path()));
        return 1;
    }
    return 0;
}

static void loadInitialSound(QQmlApplicationEngine* engine, const QUrl& url) {
    auto* root = engine->rootObjects().first();
    QMetaObject::invokeMethod(root, "loadSound", Q_ARG(QVariant, url));
}

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    Q_INIT_RESOURCE(qml);
    app.setOrganizationDomain("agateau.com");
    app.setApplicationName("sfxr-qt");
    app.setApplicationDisplayName("SFXR Qt");
    app.setWindowIcon(createIcon());
    registerQmlTypes();

    QCommandLineParser parser;
    setupCommandLineParser(&parser);
    parser.process(*qApp);

    auto maybeArgs = Arguments::parse(parser);
    if (maybeArgs.has_value() && maybeArgs.value().export_) {
        return exportSound(maybeArgs.value());
    }

    QQmlApplicationEngine engine;
    engine.load(QUrl(QStringLiteral("qrc:/main.qml")));

    if (maybeArgs.has_value()) {
        loadInitialSound(&engine, maybeArgs.value().url);
    }

    return app.exec();
}
