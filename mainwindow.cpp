#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QDebug>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    //resize(510,280);

    setFixedSize(570,280);

    ui->lblStatus->setText("Servidor parado");
    ui->lblClientes->setText("Clientes conectados: 0");

    ui->btnEnviar->setEnabled(false);
    ui->txtMensagem->setEnabled(false);
    ui->txtMensagens->setReadOnly(true);

    chatServer = new ChatServer(this);

    connect(chatServer, &ChatServer::listaUsuariosAlterada,
            this, [this](const QStringList& usuarios)
            {
                ui->lstUsuarios->clear();
                ui->lstUsuarios->addItems(usuarios);
            });

    connect(chatServer, &ChatServer::statusServidorAlterado,
            this, [this](bool ativo)
            {
                ui->lblStatus->setText(
                    ativo
                        ? "Servidor ativo — Porta 5000"
                        : "Servidor parado"
                    );
            });

    connect(chatServer, &ChatServer::quantidadeClientesAlterada,
            this, [this](int quantidade)
            {
                ui->lblClientes->setText(
                    QString("Clientes conectados: %1")
                        .arg(quantidade)
                    );
            });

    if (!chatServer->iniciar(5000))
    {
        qDebug() << "Erro ao iniciar servidor";
    }
    else
    {
        qDebug() << "Servidor escutando na porta 5000";

        ui->btnEnviar->setEnabled(true);
        ui->txtMensagem->setEnabled(true);
    }

    connect(chatServer, &ChatServer::clienteConectado,
            this, []()
            {
                qDebug() << "Cliente conectado!";
            });

    connect(chatServer, &ChatServer::mensagemRecebida,
            this, [this](const QString& mensagem)
            {
                ui->txtMensagens->appendPlainText(
                    mensagem.trimmed()
                    );
            });

    connect(ui->btnEnviar, &QPushButton::clicked,
            this, [this]()
            {
                QString mensagem = ui->txtMensagem->text();

                if (mensagem.isEmpty())
                    return;

                chatServer->enviarMensagemServidor(mensagem);

                ui->txtMensagens->appendPlainText(
                    "Servidor: " + mensagem
                    );

                ui->txtMensagem->clear();
            });
}

MainWindow::~MainWindow()
{
    delete ui;
}