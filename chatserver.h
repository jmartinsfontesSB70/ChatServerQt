#ifndef CHATSERVER_H
#define CHATSERVER_H

#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QList>
#include <QHash>
#include <QByteArray>
#include <QStringList>

struct Cliente
{
    QTcpSocket *socket;
    QString nickname;
};

class ChatServer : public QObject
{
    Q_OBJECT

public:
    explicit ChatServer(QObject *parent = nullptr);
    bool iniciar(quint16 porta);
    void enviarMensagemServidor(const QString& mensagem);

signals:
    void clienteConectado();
    void mensagemRecebida(const QString& mensagem);
    void statusServidorAlterado(bool ativo);
    void quantidadeClientesAlterada(int quantidade);
    void listaUsuariosAlterada(const QStringList& usuarios);

private:
    void aceitarConexao();
    void processarDados(QTcpSocket *cliente);
    void transmitirParaTodos(const QString& mensagem);
    void transmitirListaUsuarios();

    QTcpServer *servidor;
    QList<QTcpSocket*> clientes;
    QList<Cliente> clientesAutorizados;

    // Buffer individual para cada conexão TCP.
    QHash<QTcpSocket*, QByteArray> buffersRecebimento;
};

#endif // CHATSERVER_H