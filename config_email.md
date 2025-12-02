# 📧 CONFIGURAÇÃO DE E-MAIL PARA ENVIO DE REQUISIÇÕES DE MANUTENÇÃO
**Sistema de Controle de Acesso ESP32-S3 - Versão 6.0.54**

---

## 📋 ÍNDICE

1. [Visão Geral](#visão-geral)
2. [Acesso à Configuração](#acesso-à-configuração)
3. [Campos de Configuração](#campos-de-configuração)
4. [Configuração Gmail (Recomendado)](#configuração-gmail-recomendado)
5. [Passo a Passo Completo](#passo-a-passo-completo)
6. [Verificação da Configuração](#verificação-da-configuração)
7. [Troubleshooting](#troubleshooting)
8. [Informações Técnicas](#informações-técnicas)

---

## 🎯 VISÃO GERAL

A tela **E-MAIL** permite configurar os dados de envio automático de requisições de manutenção do sistema. Uma vez configurado, o sistema enviará e-mails automaticamente quando um técnico solicitar manutenção através da interface.

### ⚙️ Funcionalidades:
- ✅ Configuração de e-mail destinatário (quem receberá as requisições)
- ✅ Configuração de e-mail remetente (conta SMTP que enviará)
- ✅ Senha segura (App Password)
- ✅ Persistência automática no NVS (não-volátil)
- ✅ Validação de campos

---

## 🔐 ACESSO À CONFIGURAÇÃO

### **Passo 1: Autenticação Admin**

1. Na tela inicial, clique no botão **⚙️ CONFIG**
2. Digite o PIN admin (padrão: `9999`)
3. Clique em **✓ OK**

### **Passo 2: Navegar para E-MAIL**

1. Na tela de configurações, você verá 5 abas:
   ```
   [ CAL ] [ WIFI ] [ HORA ] [ E-MAIL ] [ ADMIN ]
   ```
2. Clique na aba **📧 E-MAIL**

---

## 📝 CAMPOS DE CONFIGURAÇÃO

A tela E-MAIL possui **3 campos** principais:

### 1️⃣ **DESTINATÁRIO (E-mail para receber requisições)**

| Campo | Descrição |
|-------|-----------|
| **Nome** | `DESTINATÁRIO (Recebe as requisições):` |
| **Tipo** | E-mail válido |
| **Exemplo** | `manutencao@empresa.com` |
| **Limite** | 64 caracteres |
| **Obrigatório** | ✅ Sim |

**Função:** E-mail que receberá todas as requisições de manutenção enviadas pelo sistema.

---

### 2️⃣ **REMETENTE (Login SMTP)**

| Campo | Descrição |
|-------|-----------|
| **Nome** | `REMETENTE (SMTP Login):` |
| **Tipo** | E-mail válido (conta Gmail recomendada) |
| **Exemplo** | `sistema.acesso@gmail.com` |
| **Limite** | 64 caracteres |
| **Obrigatório** | ✅ Sim |

**Função:** Conta de e-mail que o ESP32 usará para enviar as mensagens.

---

### 3️⃣ **SENHA (App Password)**

| Campo | Descrição |
|-------|-----------|
| **Nome** | `SENHA (App Password):` |
| **Tipo** | Senha de aplicativo (16 caracteres para Gmail) |
| **Exemplo** | `abcd efgh ijkl mnop` |
| **Limite** | 32 caracteres |
| **Obrigatório** | ✅ Sim |
| **Modo** | 🔒 Senha (oculta com asteriscos) |

**Função:** Senha de aplicativo gerada pela conta remetente (não use a senha normal!).

---

## 📧 CONFIGURAÇÃO GMAIL (RECOMENDADO)

O Gmail é a opção mais confiável e testada. Siga estas instruções:

### **1. Criar/Usar Conta Gmail**

Você pode:
- ✅ Criar uma conta nova exclusiva para o sistema (recomendado)
- ✅ Usar uma conta existente

**Exemplo de conta dedicada:**
```
E-mail: sistema.controleacesso@gmail.com
Propósito: Envio automático de requisições
```

---

### **2. Ativar Verificação em 2 Etapas**

A verificação em 2 etapas é **OBRIGATÓRIA** para gerar senhas de aplicativo.

#### **Passo a passo:**

1. Acesse: [https://myaccount.google.com/security](https://myaccount.google.com/security)
2. Role até **"Como fazer login no Google"**
3. Clique em **"Verificação em duas etapas"**
4. Siga as instruções para ativar:
   - Configure um número de telefone
   - Confirme o código recebido
   - Ative a verificação

✅ **Confirmação:** Você verá um badge azul "Ativada"

---

### **3. Gerar Senha de Aplicativo (App Password)**

⚠️ **IMPORTANTE:** Nunca use sua senha normal do Gmail! Use uma senha de aplicativo.

#### **Passo a passo:**

1. Acesse: [https://myaccount.google.com/apppasswords](https://myaccount.google.com/apppasswords)
2. Faça login se solicitado
3. Em **"Selecionar app"**, escolha: **"Outro (nome personalizado)"**
4. Digite um nome: `ESP32 Controle Acesso`
5. Clique em **"GERAR"**
6. **COPIE A SENHA** exibida (16 caracteres, geralmente com espaços)

**Exemplo de senha gerada:**
```
abcd efgh ijkl mnop
```

⚠️ **ATENÇÃO:**
- Esta senha será exibida **apenas UMA VEZ**
- Guarde-a em local seguro
- Se perder, delete e gere uma nova

---

## 🚀 PASSO A PASSO COMPLETO

### **CONFIGURAÇÃO COMPLETA DO SISTEMA**

#### **1️⃣ Acesse a Tela E-MAIL**
```
Tela Inicial → CONFIG (PIN 9999) → Aba E-MAIL
```

---

#### **2️⃣ Configure DESTINATÁRIO**

1. **Toque no campo:** `DESTINATÁRIO (Recebe as requisições):`
2. **Abrirá teclado virtual** com título:
   ```
   ┌─────────────────────────────────────┐
   │ E-mail Destinatario:                │
   │ ┌─────────────────────────────────┐ │
   │ │                                 │ │
   │ └─────────────────────────────────┘ │
   │  [Teclado QWERTY]                   │
   │  [CANCELAR]           [CONFIRMAR]   │
   └─────────────────────────────────────┘
   ```

3. **Digite o e-mail** que receberá as requisições:
   ```
   Exemplo: manutencao@empresa.com
   ```

4. **Clique em CONFIRMAR** (canto inferior direito)
5. ✅ Tela volta automaticamente para CONFIG
6. ✅ Campo mostra o e-mail configurado

---

#### **3️⃣ Configure REMETENTE**

1. **Toque no campo:** `REMETENTE (SMTP Login):`
2. **Digite a conta Gmail** que enviará os e-mails:
   ```
   Exemplo: sistema.acesso@gmail.com
   ```

3. **Clique em CONFIRMAR**
4. ✅ Valor salvo e exibido no campo

---

#### **4️⃣ Configure SENHA**

1. **Toque no campo:** `SENHA (App Password):`
2. **Digite a senha de aplicativo** gerada pelo Gmail:
   ```
   Exemplo: abcd efgh ijkl mnop
   ```
   
   💡 **Dica:** Você pode copiar/colar ou digitar manualmente

3. **Clique em CONFIRMAR**
4. ✅ Campo mostra asteriscos: `****************`

---

#### **5️⃣ Salvar Configuração (Opcional)**

O sistema salva **automaticamente** ao confirmar cada campo.

Você pode clicar no botão verde:
```
┌─────────────────────────────────────┐
│    💾 SALVAR CONFIGURACAO           │
└─────────────────────────────────────┘
```

Isso exibe uma mensagem de confirmação.

---

## ✅ VERIFICAÇÃO DA CONFIGURAÇÃO

### **Como verificar se está configurado:**

1. Entre na tela **CONFIG → E-MAIL**
2. Verifique se os campos mostram valores:

```
┌─────────────────────────────────────────┐
│ DESTINATÁRIO (Recebe as requisições):   │
│ ┌─────────────────────────────────────┐ │
│ │ manutencao@empresa.com              │ │
│ └─────────────────────────────────────┘ │
│                                         │
│ REMETENTE (SMTP Login):                 │
│ ┌─────────────────────────────────────┐ │
│ │ sistema.acesso@gmail.com            │ │
│ └─────────────────────────────────────┘ │
│                                         │
│ SENHA (App Password):                   │
│ ┌─────────────────────────────────────┐ │
│ │ ****************                    │ │
│ └─────────────────────────────────────┘ │
└─────────────────────────────────────────┘
```

✅ **Campos preenchidos = Configuração completa**

---

### **Testar Envio:**

1. Acesse a tela **🔧 MANUTENÇÃO**
2. Preencha uma requisição de teste:
   - **Local:** `Teste de Configuração`
   - **Problema:** `Testando envio de e-mail`
   - **Prioridade:** `Média`
   - **Contato:** `teste@teste.com`

3. Clique em **ENVIAR**
4. Aguarde a confirmação na tela
5. Verifique se o e-mail chegou em `manutencao@empresa.com`

---

## 🔧 TROUBLESHOOTING

### ❌ **Problema: "Erro ao enviar e-mail"**

**Causas possíveis:**

1. **Senha incorreta**
   - ✅ Certifique-se de usar App Password, não a senha normal
   - ✅ Regenere a senha de aplicativo se necessário

2. **WiFi desconectado**
   - ✅ Verifique conexão WiFi na tela CONFIG → WIFI
   - ✅ Teste conectividade

3. **E-mail remetente inválido**
   - ✅ Use uma conta Gmail válida
   - ✅ Verifique se não há erros de digitação

---

### ❌ **Problema: "Não recebo os e-mails"**

**Verificações:**

1. **Caixa de SPAM**
   - ✅ Verifique a pasta de spam/lixo eletrônico
   - ✅ Marque como "não é spam"

2. **E-mail destinatário incorreto**
   - ✅ Verifique se digitou corretamente
   - ✅ Teste com outro e-mail

3. **Filtros de e-mail**
   - ✅ Desabilite temporariamente filtros automáticos
   - ✅ Adicione remetente à lista de contatos seguros

---

### ❌ **Problema: "Campos não salvam"**

**Solução:**

1. **Sempre clique CONFIRMAR** no teclado virtual
2. **Não pressione CANCELAR** se quiser salvar
3. **Aguarde** a tela voltar para CONFIG antes de editar outro campo

---

### ⚠️ **Problema: "Senha de aplicativo não aceita"**

**Soluções:**

1. **Delete a senha antiga:**
   - Acesse: [https://myaccount.google.com/apppasswords](https://myaccount.google.com/apppasswords)
   - Remova a senha antiga
   - Gere uma nova

2. **Verifique verificação em 2 etapas:**
   - Deve estar **ativada**
   - Se desativada, senhas de aplicativo não funcionam

3. **Digite sem espaços:**
   ```
   ❌ Errado: abcd efgh ijkl mnop
   ✅ Correto: abcdefghijklmnop
   
   (Ambos funcionam, mas sem espaços é mais confiável)
   ```

---

## 🔬 INFORMAÇÕES TÉCNICAS

### **Armazenamento:**

```cpp
Namespace NVS: "email_config"

Chaves:
- "recipient"      → E-mail destinatário
- "smtp_email"     → E-mail remetente (SMTP)
- "smtp_password"  → Senha de aplicativo
```

**Capacidade:**
- Cada campo: até 64 caracteres (destinatário/remetente) ou 32 (senha)
- Armazenamento: Flash NVS (não-volátil)
- Persistência: Mantém configuração após reboot

---

### **Servidor SMTP Gmail:**

```
Servidor: smtp.gmail.com
Porta: 465 (SSL) ou 587 (TLS)
Autenticação: REQUIRED
Tipo: SMTP com autenticação
```

---

### **Formato do E-mail Enviado:**

```
De: sistema.acesso@gmail.com
Para: manutencao@empresa.com
Assunto: 🔧 Requisição de Manutenção #001

════════════════════════════════════════
       🔧 REQUISIÇÃO DE MANUTENÇÃO
════════════════════════════════════════

📋 ID: #001
📅 Data: 01/12/2025 13:45:30
🏢 Local: Sala de Servidores
⚠️ Prioridade: Alta

📝 PROBLEMA:
Ar condicionado não está refrigerando

📞 CONTATO:
tecnico@empresa.com

════════════════════════════════════════
   Sistema de Controle de Acesso v6.0
════════════════════════════════════════
```

---

### **Segurança:**

✅ **Senhas criptografadas** no NVS  
✅ **Modo senha** oculta caracteres na tela  
✅ **App Password** evita exposição da senha principal  
✅ **Autenticação admin** para acessar configurações  

⚠️ **Recomendações:**
- Use conta Gmail dedicada
- Não compartilhe App Password
- Mude PIN admin padrão (9999)
- Monitore e-mails enviados

---

## 📌 RESUMO RÁPIDO

| Passo | Ação |
|-------|------|
| 1️⃣ | Criar conta Gmail dedicada |
| 2️⃣ | Ativar verificação em 2 etapas |
| 3️⃣ | Gerar senha de aplicativo |
| 4️⃣ | Acessar CONFIG → E-MAIL no ESP32 |
| 5️⃣ | Configurar DESTINATÁRIO |
| 6️⃣ | Configurar REMETENTE (Gmail) |
| 7️⃣ | Configurar SENHA (App Password) |
| 8️⃣ | Testar enviando requisição |

---

## 📞 SUPORTE

**Em caso de dúvidas:**

1. Consulte esta documentação
2. Verifique seção [Troubleshooting](#troubleshooting)
3. Teste com conta Gmail nova
4. Verifique logs do monitor serial

**Logs úteis:**
```bash
pio device monitor
```

Procure por:
```
✅ [Email] Conectado ao servidor SMTP
✅ [Email] E-mail enviado com sucesso
❌ [Email] Erro: [descrição do erro]
```

---

## 📝 CHANGELOG

### **v6.0.54 (01/12/2025)**
- ✅ Implementação inicial da tela E-MAIL
- ✅ 3 campos configuráveis (destinatário, remetente, senha)
- ✅ Teclado virtual dedicado
- ✅ Persistência automática no NVS
- ✅ Botões CONFIRMAR/CANCELAR funcionais
- ✅ Integração com sistema de manutenção

---

## ✅ CONCLUSÃO

A configuração de e-mail é **simples e rápida** seguindo este guia. Com Gmail e senha de aplicativo, o sistema enviará automaticamente todas as requisições de manutenção para o e-mail configurado.

**Tempo estimado de configuração:** 5-10 minutos

**Pré-requisitos:**
- ✅ Conta Gmail
- ✅ Verificação em 2 etapas ativada
- ✅ Senha de aplicativo gerada
- ✅ Conexão WiFi configurada

---

**Desenvolvido por:** Sistema de Controle de Acesso ESP32-S3  
**Versão:** 6.0.54  
**Data:** Dezembro 2025  
**Plataforma:** ESP32-S3-WROOM-N8R8 + LVGL 8.4.0

---

