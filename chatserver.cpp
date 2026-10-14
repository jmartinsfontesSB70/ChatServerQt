#include "chatserver.h"
#include <QDebug>
#include <QJsonArray>
#include <QJsonDocument>
#include <QStringList>

ChatServer::ChatServer(QObject *parent)
    : QObject(parent)
{
    servidor = new QTcpServer(this);

    connect(servidor, &QTcpServer::newConnection,
            this, &ChatServer::aceitarConexao);
}

bool ChatServer::iniciar(quint16 porta)
{
    bool iniciado = servidor->listen(
        QHostAddress::AnyIPv4,
        porta
        );

    emit statusServidorAlterado(iniciado);

    return iniciado;
}

void ChatServer::aceitarConexao()
{
    QTcpSocket *cliente =
        servidor->nextPendingConnection();

    clientes.append(cliente);

    buffersRecebimento.insert(cliente, QByteArray());

    emit quantidadeClientesAlterada(clientes.size());

    connect(cliente, &QTcpSocket::readyRead,
            this, [this, cliente]()
            {
                processarDados(cliente);
            });


    connect(cliente, &QTcpSocket::disconnected,
            this, [this, cliente]()
            {
                qDebug() << "Cliente desconectado:" << cliente;

                // Remove o socket da lista de conexões.
                clientes.removeAll(cliente);

                emit quantidadeClientesAlterada(clientes.size());

                // Remove o nickname do cliente desconectado.
                bool nicknameRemovido = false;

                for (int i = 0; i < clientesAutorizados.size(); ++i)
                {
                    if (clientesAutorizados[i].socket == cliente)
                    {
                        qDebug() << "Nickname liberado:"
                                 << clientesAutorizados[i].nickname;

                        clientesAutorizados.removeAt(i);
                        nicknameRemovido = true;
                        break;
                    }
                }

                // Libera o buffer associado à conexão.
                buffersRecebimento.remove(cliente);

                // Atualiza a lista dos clientes que continuam conectados.
                if (nicknameRemovido)
                {
                    transmitirListaUsuarios();
                }

                // Libera os recursos do socket.
                cliente->deleteLater();
            });


    emit clienteConectado();
}


void ChatServer::processarDados(QTcpSocket *cliente)
{
    // 1. Recebe os bytes disponíveis e acumula no buffer
    // específico desta conexão.
    buffersRecebimento[cliente].append(cliente->readAll());

    // 2. Processa todas as mensagens completas que chegaram.
    while (true)
    {
        QByteArray& buffer = buffersRecebimento[cliente];

        // Ainda não chegou o delimitador: aguardamos mais dados.
        qsizetype pos = buffer.indexOf('\n');

        if (pos == -1)
        {
            break;
        }

        // Extrai somente a próxima mensagem completa.
        QByteArray linha = buffer.left(pos);

        // Remove a mensagem e o '\n' do buffer.
        buffer.remove(0, pos + 1);

        // Converte a linha para texto e preserva o comportamento
        // anterior de remover espaços nas extremidades.
        QString texto = QString::fromUtf8(linha).trimmed();

        qDebug() << "Mensagem completa recebida:" << texto;

        // Verifica se o cliente já tem nickname autorizado.
        bool clienteAutorizado = false;

        for (const Cliente& c : clientesAutorizados)
        {
            if (c.socket == cliente)
            {
                clienteAutorizado = true;
                break;
            }
        }

        // 3. Se ainda não está autorizado, esta linha é o nickname.
        if (!clienteAutorizado)
        {
            if (texto.isEmpty())
            {
                cliente->write("NICK_EMPTY\n");
                continue;
            }

            bool nicknameDuplicado = false;

            for (const Cliente& c : clientesAutorizados)
            {
                if (c.nickname.compare(
                        texto, Qt::CaseInsensitive) == 0)
                {
                    nicknameDuplicado = true;
                    break;
                }
            }

            if (nicknameDuplicado)
            {
                cliente->write("NICK_TAKEN\n");

                qDebug() << "Nickname recusado:" << texto;

                continue;
            }

            cliente->write("NICK_OK\n");

            clientesAutorizados.append({
                cliente,
                texto
            });

            qDebug() << "Nickname aceito:" << texto;

            transmitirListaUsuarios();

            continue;
        }

        // 4. Cliente autorizado: recupera seu nickname.
        QString nickname;

        for (const Cliente& c : clientesAutorizados)
        {
            if (c.socket == cliente)
            {
                nickname = c.nickname;
                break;
            }
        }

        // Verifica se a mensagem é privada.
        if (texto.startsWith("PRIV|"))
        {
            // Localiza o separador após o destinatário.
            qsizetype separador = texto.indexOf('|', 5);

            if (separador == -1)
            {
                cliente->write(
                    "PRIV_ERROR|Formato de mensagem invalido\n");
                continue;
            }

            QString destinatario = texto.mid(
                                            5, separador - 5).trimmed();

            QString conteudo = texto.mid(separador + 1);

            if (destinatario.isEmpty() || conteudo.isEmpty())
            {
                cliente->write(
                    "PRIV_ERROR|Destinatario ou mensagem vazia\n");
                continue;
            }

            // Procura o destinatário entre os usuários autorizados.
            QTcpSocket *socketDestinatario = nullptr;

            for (const Cliente& c : clientesAutorizados)
            {
                if (c.nickname.compare(
                        destinatario, Qt::CaseInsensitive) == 0)
                {
                    socketDestinatario = c.socket;
                    break;
                }
            }

            // O destinatário não está conectado ou autorizado.
            if (socketDestinatario == nullptr)
            {
                QByteArray erro =
                    QString("PRIV_ERROR|Usuario '%1' indisponivel\n")
                        .arg(destinatario)
                        .toUtf8();

                cliente->write(erro);

                qDebug() << "Destinatario privado indisponivel:"
                         << destinatario;

                continue;
            }

            // Encaminha a mensagem somente ao destinatário.
            QString mensagemPrivada =
                "PRIV_MSG|" + nickname + "|" + conteudo + "\n";

            socketDestinatario->write(
                mensagemPrivada.toUtf8());

            // Confirma ao remetente que a mensagem foi encaminhada.
            QString confirmacao =
                "PRIV_SENT|" + destinatario + "|" + conteudo + "\n";

            cliente->write(confirmacao.toUtf8());

            qDebug() << "Mensagem privada de"
                     << nickname << "para" << destinatario;

            continue;
        }

        // Mensagem pública: mantém o comportamento atual.
        QString mensagem = nickname + ": " + texto + "\n";

        qDebug() << "Mensagem de chat:" << mensagem;

        emit mensagemRecebida(mensagem);

        transmitirParaTodos(mensagem);

    }
}

void ChatServer::transmitirParaTodos(
    const QString& mensagem)
{
    QByteArray dados = mensagem.toUtf8();

    for (const Cliente& cliente : clientesAutorizados)
    {
        cliente.socket->write(dados);
    }
}

void ChatServer::enviarMensagemServidor(const QString& mensagem)
{
    QString texto = "Servidor: " + mensagem + "\n";

    transmitirParaTodos(texto);
}

void ChatServer::transmitirListaUsuarios()
{
    QJsonArray usuariosJson;
    QStringList usuarios;

    // Monta a lista oficial de nicknames autorizados.
    for (const Cliente& cliente : clientesAutorizados)
    {
        usuarios.append(cliente.nickname);
        usuariosJson.append(cliente.nickname);
    }

    // Atualiza a interface do servidor.
    emit listaUsuariosAlterada(usuarios);

    // Prepara a mensagem do protocolo para os clientes.
    QByteArray dados =
        "USERS:" +
        QJsonDocument(usuariosJson).toJson(
            QJsonDocument::Compact) +
        "\n";

    // Envia a lista aos clientes autorizados.
    for (const Cliente& cliente : clientesAutorizados)
    {
        cliente.socket->write(dados);
    }

    qDebug() << "Lista de usuarios enviada:" << usuarios;
}
