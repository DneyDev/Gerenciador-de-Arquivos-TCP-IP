#include <winsock2.h>
#include <windows.h>
#include <ws2tcpip.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Definição para evitar conflitos de headers
#define WIN32_LEAN_AND_MEAN

// Certifique-se que a biblioteca Winsock seja linkada
#pragma comment(lib, "ws2_32.lib")

#define DEFAULT_PORT "27015"
#define DEFAULT_BUFLEN 4096

typedef struct {
    char comando;         // Tipo de comando: 'L' (listar), 'D' (download), 'U' (upload), 'R' (remover)
    char caminho[MAX_PATH]; // Caminho do arquivo ou diretório
    DWORD tamanho;        // Tamanho do arquivo (para upload/download)
} ComandoArquivo;

// Função para listar arquivos do servidor
void listarArquivos(SOCKET serverSocket, const char* diretorio) {
    ComandoArquivo comando;
    char buffer[DEFAULT_BUFLEN];
    int bytesRecebidos;

    // Prepara o comando
    comando.comando = 'L';
    // Substituído strcpy_s por strncpy
    strncpy(comando.caminho, diretorio, MAX_PATH - 1);
    comando.caminho[MAX_PATH - 1] = '\0'; // Garantir terminação nula
    comando.tamanho = 0;

    // Envia o comando para o servidor
    send(serverSocket, (char*)&comando, sizeof(ComandoArquivo), 0);

    // Recebe e exibe a resposta do servidor
    printf("Listagem de arquivos em %s:\n", diretorio);
    printf("------------------------------------------\n");

    while (1) {
        bytesRecebidos = recv(serverSocket, buffer, DEFAULT_BUFLEN - 1, 0); // Deixar espaço para \0
        if (bytesRecebidos > 0) {
            // Garante que o buffer termine com null
            buffer[bytesRecebidos] = '\0';

            // Verifica se é o fim da listagem
            if (strstr(buffer, "---FIM DA LISTAGEM---") != NULL) {
                // Imprime a parte antes do marcador, se houver
                char* marcador = strstr(buffer, "---FIM DA LISTAGEM---");
                if (marcador != buffer) {
                    printf("%.*s", (int)(marcador - buffer), buffer);
                }
                break;
            }

            // Exibe os dados recebidos
            printf("%s", buffer);
        } else if (bytesRecebidos == 0) {
            printf("Conexão fechada pelo servidor\n");
            break;
        } else {
            printf("Erro na recepcao: %d\n", WSAGetLastError());
            break;
        }
    }

    printf("------------------------------------------\n");
}

// Função para baixar um arquivo do servidor
void baixarArquivo(SOCKET serverSocket, const char* caminhoRemoto, const char* caminhoLocal) {
    ComandoArquivo comando;
    char buffer[DEFAULT_BUFLEN];
    int bytesRecebidos;
    FILE* arquivo = NULL;
    DWORD tamanhoArquivo = 0;
    DWORD bytesRecebidosTotal = 0;

    // Prepara o comando
    comando.comando = 'D';
    // Substituído strcpy_s por strncpy
    strncpy(comando.caminho, caminhoRemoto, MAX_PATH - 1);
    comando.caminho[MAX_PATH - 1] = '\0'; // Garantir terminação nula
    comando.tamanho = 0;

    // Envia o comando para o servidor
    send(serverSocket, (char*)&comando, sizeof(ComandoArquivo), 0);

    // Recebe a resposta inicial do servidor
    bytesRecebidos = recv(serverSocket, buffer, DEFAULT_BUFLEN - 1, 0);
    if (bytesRecebidos <= 0) {
        printf("Erro ao receber resposta do servidor ou conexao fechada\n");
        return;
    }

    buffer[bytesRecebidos] = '\0';

    // Verifica se houve erro
    if (strncmp(buffer, "ERRO:", 5) == 0) {
        printf("%s", buffer);
        return;
    }

    // Extrai o tamanho do arquivo
    if (strncmp(buffer, "TAMANHO:", 8) == 0) {
        // Substituído sscanf_s por sscanf
        if (sscanf(buffer + 8, "%u", &tamanhoArquivo) != 1) {
             printf("Erro ao extrair tamanho do arquivo da resposta: %s\n", buffer);
             return;
        }
        printf("Tamanho do arquivo: %u bytes\n", tamanhoArquivo);
    } else {
        printf("Resposta inesperada do servidor (esperava TAMANHO:): %s\n", buffer);
        return;
    }

    // Abre o arquivo para escrita - Substituído fopen_s por fopen
    arquivo = fopen(caminhoLocal, "wb");
    if (arquivo == NULL) {
        printf("Erro ao criar o arquivo local: %s\n", caminhoLocal);
        // Tentar receber o restante dos dados para limpar o socket?
        // Por enquanto, apenas retorna.
        return;
    }

    printf("Baixando arquivo...\n");

    // Recebe o conteúdo do arquivo
    while (bytesRecebidosTotal < tamanhoArquivo) {
        int bytesAReceber = DEFAULT_BUFLEN;
        if (tamanhoArquivo - bytesRecebidosTotal < bytesAReceber) {
            bytesAReceber = tamanhoArquivo - bytesRecebidosTotal;
        }

        bytesRecebidos = recv(serverSocket, buffer, bytesAReceber, 0);
        if (bytesRecebidos <= 0) {
            printf("Erro ao receber dados ou conexao fechada durante download\n");
            break;
        }

        // Escreve os dados no arquivo
        fwrite(buffer, 1, bytesRecebidos, arquivo);
        bytesRecebidosTotal += bytesRecebidos;

        // Exibe progresso
        printf("\rProgresso: %.1f%%", (bytesRecebidosTotal * 100.0) / tamanhoArquivo);
    }

    fclose(arquivo);

    // Recebe o marcador de fim de arquivo (se ainda não foi recebido)
    // É importante ler o marcador para sincronizar com o servidor
    if (bytesRecebidosTotal == tamanhoArquivo) {
         bytesRecebidos = recv(serverSocket, buffer, DEFAULT_BUFLEN -1, 0);
         if (bytesRecebidos > 0) {
             buffer[bytesRecebidos] = '\0';
             if (strstr(buffer, "---FIM DO ARQUIVO---") != NULL) {
                 printf("\nArquivo baixado com sucesso: %s\n", caminhoLocal);
             } else {
                 printf("\nDownload concluido, mas marcador final inesperado: %s\n", buffer);
             }
         } else {
              printf("\nDownload concluido, mas erro ao receber marcador final.\n");
         }
    } else {
        printf("\nDownload incompleto. Recebido %u de %u bytes.\n", bytesRecebidosTotal, tamanhoArquivo);
    }
}

// Função para enviar um arquivo para o servidor
void enviarArquivo(SOCKET serverSocket, const char* caminhoLocal, const char* caminhoRemoto) {
    ComandoArquivo comando;
    char buffer[DEFAULT_BUFLEN];
    int bytesRecebidos, bytesLidos, bytesEnviados;
    FILE* arquivo = NULL;
    DWORD tamanhoArquivo = 0;
    DWORD bytesEnviadosTotal = 0;

    // Abre o arquivo para leitura - Substituído fopen_s por fopen
    arquivo = fopen(caminhoLocal, "rb");
    if (arquivo == NULL) {
        printf("Erro ao abrir o arquivo local: %s\n", caminhoLocal);
        return;
    }

    // Obtém o tamanho do arquivo
    fseek(arquivo, 0, SEEK_END);
    tamanhoArquivo = ftell(arquivo);
    fseek(arquivo, 0, SEEK_SET);

    // Prepara o comando
    comando.comando = 'U';
    // Substituído strcpy_s por strncpy
    strncpy(comando.caminho, caminhoRemoto, MAX_PATH - 1);
    comando.caminho[MAX_PATH - 1] = '\0'; // Garantir terminação nula
    comando.tamanho = tamanhoArquivo;

    // Envia o comando para o servidor
    send(serverSocket, (char*)&comando, sizeof(ComandoArquivo), 0);

    // Aguarda confirmação do servidor
    bytesRecebidos = recv(serverSocket, buffer, DEFAULT_BUFLEN - 1, 0);
    if (bytesRecebidos <= 0) {
        printf("Erro ao receber resposta do servidor ou conexao fechada\n");
        fclose(arquivo);
        return;
    }

    buffer[bytesRecebidos] = '\0';

    // Verifica se o servidor está pronto para receber
    if (strcmp(buffer, "PRONTO_PARA_RECEBER\r\n") != 0) {
        printf("Servidor não está pronto para receber ou resposta inesperada: %s\n", buffer);
        fclose(arquivo);
        return;
    }

    printf("Enviando arquivo...\n");

    // Envia o conteúdo do arquivo
    while ((bytesLidos = (int)fread(buffer, 1, DEFAULT_BUFLEN, arquivo)) > 0) {
        bytesEnviados = send(serverSocket, buffer, bytesLidos, 0);
        if (bytesEnviados == SOCKET_ERROR) {
            printf("Erro ao enviar dados: %d\n", WSAGetLastError());
            break;
        }

        bytesEnviadosTotal += bytesEnviados;

        // Exibe progresso
        printf("\rProgresso: %.1f%%", (bytesEnviadosTotal * 100.0) / tamanhoArquivo);
    }

    fclose(arquivo);

    // Recebe confirmação final
    bytesRecebidos = recv(serverSocket, buffer, DEFAULT_BUFLEN - 1, 0);
    if (bytesRecebidos > 0) {
        buffer[bytesRecebidos] = '\0';
        printf("\nResposta do servidor: %s", buffer); // Resposta já deve ter \r\n
    } else {
        printf("\nErro ao receber confirmaçao final do servidor.\n");
    }

    // Não imprimir sucesso aqui, a resposta do servidor já informa
    // printf("\nArquivo enviado com sucesso: %s\n", caminhoRemoto);
}

// Função para remover um arquivo no servidor
void removerArquivo(SOCKET serverSocket, const char* caminhoRemoto) {
    ComandoArquivo comando;
    char buffer[DEFAULT_BUFLEN];
    int bytesRecebidos;

    // Prepara o comando
    comando.comando = 'R';
    // Substituído strcpy_s por strncpy
    strncpy(comando.caminho, caminhoRemoto, MAX_PATH - 1);
    comando.caminho[MAX_PATH - 1] = '\0'; // Garantir terminação nula
    comando.tamanho = 0;

    // Envia o comando para o servidor
    send(serverSocket, (char*)&comando, sizeof(ComandoArquivo), 0);

    // Recebe a resposta do servidor
    bytesRecebidos = recv(serverSocket, buffer, DEFAULT_BUFLEN - 1, 0);
    if (bytesRecebidos > 0) {
        buffer[bytesRecebidos] = '\0';
        printf("Resposta do servidor: %s", buffer);
    } else {
        printf("Erro ao receber resposta do servidor ou conexao fechada\n");
    }
}

// Função para exibir o menu de opções
void exibirMenu() {
    printf("\n===== GERENCIADOR DE ARQUIVOS - CLIENTE =====\n");
    printf("1. Listar arquivos\n");
    printf("2. Baixar arquivo\n");
    printf("3. Enviar arquivo\n");
    printf("4. Remover arquivo\n");
    printf("0. Sair\n");
    printf("Escolha uma opcao: ");
}

// Função segura para ler string do usuário (substitui gets_s e problemas com scanf)
void lerString(char* buffer, int tamanho) {
    if (fgets(buffer, tamanho, stdin) != NULL) {
        // Remove o caractere de nova linha se existir
        size_t len = strlen(buffer);
        if (len > 0 && buffer[len - 1] == '\n') {
            buffer[len - 1] = '\0';
        } else {
            // Limpa o restante do buffer de entrada se a linha for muito longa
            int ch;
            while ((ch = getchar()) != '\n' && ch != EOF);
        }
    } else {
        // Em caso de erro ou EOF, garante que a string esteja vazia
        buffer[0] = '\0';
    }
}

int main(int argc, char* argv[]) {
    WSADATA wsaData;
    SOCKET connectSocket = INVALID_SOCKET;
    // Usando sockaddr_in em vez de getaddrinfo para maior compatibilidade
    struct sockaddr_in serverAddr;
    int iResult;
    char serverIP[16] = "127.0.0.1"; // IP padrão (localhost)

    // Verifica se o IP do servidor foi fornecido
    if (argc > 1) {
        // Substituído strcpy_s por strncpy
        strncpy(serverIP, argv[1], 15);
        serverIP[15] = '\0'; // Garantir terminação nula
    }

    // Inicializa Winsock
    iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (iResult != 0) {
        printf("WSAStartup falhou: %d\n", iResult);
        return 1;
    }

    // Cria o socket para conectar ao servidor
    connectSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (connectSocket == INVALID_SOCKET) {
        printf("Erro ao criar socket: %d\n", WSAGetLastError());
        WSACleanup();
        return 1;
    }

    // Configura o endereço do servidor
    memset(&serverAddr, 0, sizeof(serverAddr)); // Zera a estrutura
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons((unsigned short)atoi(DEFAULT_PORT)); // Converte porta para número e network byte order

    // Converte o endereço IP de string para formato binário - Usando inet_addr para maior compatibilidade
    serverAddr.sin_addr.s_addr = inet_addr(serverIP);
    if (serverAddr.sin_addr.s_addr == INADDR_NONE) {
        printf("Endereço IP invalido ou erro na conversao: %s\n", serverIP);
        closesocket(connectSocket);
        WSACleanup();
        return 1;
    }

    // Conecta ao servidor
    iResult = connect(connectSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr));
    if (iResult == SOCKET_ERROR) {
        printf("Nao foi possível conectar ao servidor %s:%s. Erro: %d\n", serverIP, DEFAULT_PORT, WSAGetLastError());
        closesocket(connectSocket);
        WSACleanup();
        return 1;
    }

    printf("Conectado ao servidor %s:%s\n", serverIP, DEFAULT_PORT);

    // Loop principal do cliente
    int opcao = -1;
    char caminhoRemoto[MAX_PATH];
    char caminhoLocal[MAX_PATH];
    char bufferEntrada[10]; // Buffer para ler a opção do menu

    while (opcao != 0) {
        exibirMenu();

        // Substituído scanf_s por lerString + sscanf para segurança
        lerString(bufferEntrada, sizeof(bufferEntrada));
        if (sscanf(bufferEntrada, "%d", &opcao) != 1) {
            opcao = -1; // Define como inválido se a conversão falhar
        }

        switch (opcao) {
            case 1: // Listar arquivos
                printf("Digite o diretorio remoto (ou . para atual): ");
                // Substituído gets_s por lerString
                lerString(caminhoRemoto, MAX_PATH);
                listarArquivos(connectSocket, caminhoRemoto);
                break;

            case 2: // Baixar arquivo
                printf("Digite o caminho do arquivo remoto: ");
                // Substituído gets_s por lerString
                lerString(caminhoRemoto, MAX_PATH);
                printf("Digite o caminho para salvar localmente: ");
                // Substituído gets_s por lerString
                lerString(caminhoLocal, MAX_PATH);
                baixarArquivo(connectSocket, caminhoRemoto, caminhoLocal);
                break;

            case 3: // Enviar arquivo
                printf("Digite o caminho do arquivo local: ");
                // Substituído gets_s por lerString
                lerString(caminhoLocal, MAX_PATH);
                printf("Digite o caminho para salvar no servidor: ");
                // Substituído gets_s por lerString
                lerString(caminhoRemoto, MAX_PATH);
                enviarArquivo(connectSocket, caminhoLocal, caminhoRemoto);
                break;

            case 4: // Remover arquivo
                printf("Digite o caminho do arquivo remoto a ser removido: ");
                // Substituído gets_s por lerString
                lerString(caminhoRemoto, MAX_PATH);
                removerArquivo(connectSocket, caminhoRemoto);
                break;

            case 0: // Sair
                printf("Encerrando cliente...\n");
                break;

            default:
                printf("Opção inválida!\n");
                break;
        }
    }

    // Fecha a conexão
    closesocket(connectSocket);
    WSACleanup();

    return 0;
}

