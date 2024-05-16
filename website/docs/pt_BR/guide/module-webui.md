# Módulo WebUI

Além de executar scripts de inicialização e modificar arquivos do sistema, os módulos do KernelSU também suportam a exibição de interfaces da UI e a interação com os usuários.

O módulo pode escrever páginas HTML + CSS + JavaScript através de qualquer tecnologia web. O gerenciador do KernelSU exibirá essas páginas através do WebView. Ele também fornece algumas APIs para interagir com o sistema, como executar comandos shell.

## Diretório webroot

Os arquivos de recursos da web devem ser colocados no subdiretório `webroot` do diretório raiz do módulo, e **DEVE** haver um arquivo chamado `index.html`, que é a entrada da página do módulo. A estrutura do módulo mais simples contendo uma interface web é a seguinte:

```txt
❯ tree .
.
|-- module.prop
`-- webroot
    `-- index.html
```

:::warning AVISO
Ao instalar o módulo, KernelSU definirá automaticamente as permissões e o contexto do SELinux deste diretório. Se você não sabe o que está fazendo, não defina você mesmo as permissões deste diretório!
:::

Se sua página contém CSS e JavaScript, você também precisa colocá-la neste diretório.

## API JavaScript

Se for apenas uma página de exibição, não será diferente de uma página web normal. Mais importante ainda, KernelSU fornece uma série de APIs de sistema que permitem implementar as funções exclusivas do módulo.

KernelSU fornece uma biblioteca JavaScript e publica-a no [npm](https://www.npmjs.com/package/kernelsu), que você pode usar no código JavaScript de suas páginas da web.

Por exemplo, você pode executar um comando shell para obter uma configuração específica ou modificar uma propriedade:

```JavaScript
import { exec } from 'kernelsu';

const { errno, stdout } = exec("getprop ro.product.model");
```

Para outro exemplo, você pode fazer com que a página web seja exibida em tela inteira ou exibir um dica.

[Documentação da API](https://www.npmjs.com/package/kernelsu)

Se você achar que a API existente não atende às suas necessidades ou é inconveniente de usar, fique à vontade para nos dar sugestões [aqui](https://github.com/tiann/KernelSU/issues)!

## Algumas dicas

1. Você pode usar `localStorage` normalmente para armazenar alguns dados, mas eles serão perdidos após a desinstalação do app gerenciador. Se precisar salvar persistentemente, você mesmo pode gravar os dados em algum diretório.
2. Para páginas simples, recomendo que você use [parceljs](https://parceljs.org/) para o empacotamento. Não requer configuração do zero e é muito conveniente de usar. Porém, se você é um mestre do front-end ou tem suas próprias preferências, basta escolher o que você gosta!
