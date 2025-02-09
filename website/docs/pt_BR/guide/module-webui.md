# Módulo WebUI

Além de executar scripts de inicialização e modificar arquivos do sistema, os módulos do KernelSU também suportam a exibição de interfaces da UI e à interação direta com os usuários.

O módulo pode escrever páginas HTML + CSS + JavaScript através de qualquer tecnologia web. O gerenciador do KernelSU exibirá essas páginas através do WebView. Ele também fornece algumas APIs para interagir com o sistema, como executar comandos shell.

## Diretório `webroot`

Os arquivos de recursos da web devem ser colocados no subdiretório `webroot` do diretório raiz do módulo, e **DEVE** haver um arquivo chamado `index.html`, que é a entrada da página do módulo. A estrutura do módulo mais simples contendo uma interface web é a seguinte:

```txt
❯ tree .
.
|-- module.prop
`-- webroot
    `-- index.html
```

::: warning AVISO
Ao instalar o módulo, KernelSU definirá automaticamente as permissões e o contexto do SELinux deste diretório. Se você não sabe o que está fazendo, não defina você mesmo as permissões deste diretório!
:::

Se sua página contém CSS e JavaScript, você também precisa colocá-la neste diretório.

## API JavaScript

Se for apenas uma página de exibição, ela funcionará como uma página web comum. No entanto, o mais importante é que o KernelSU oferece uma série de APIs de sistema, permitindo a implementação de funções exclusivas do módulo.

O KernelSU disponibiliza uma biblioteca JavaScript, que está publicada no [npm](https://www.npmjs.com/package/kernelsu) e pode ser usada no código JavaScript das suas páginas web.

Por exemplo, você pode executar um comando shell para obter uma configuração específica ou modificar uma propriedade:

```JavaScript
import { exec } from 'kernelsu';

const { errno, stdout } = exec("getprop ro.product.model");
```

Para outro exemplo, você pode fazer com que a página web seja exibida em tela inteira ou exibir um dica.

[Documentação da API](https://www.npmjs.com/package/kernelsu)

Se você achar que a API existente não atende às suas necessidades ou é inconveniente de usar, fique à vontade para nos dar sugestões [aqui](https://github.com/tiann/KernelSU/issues)!

## Algumas dicas

1. Você pode usar `localStorage` normalmente para armazenar alguns dados, mas tenha em mente que eles serão perdidos caso o app gerenciador seja desinstalado. Se precisar de armazenamento persistente, será necessário gravar os dados manualmente em algum diretório.
2. Para páginas simples, recomendamos o uso do [parceljs](https://parceljs.org/) para empacotamento. Ele não exige configuração inicial e é extremamente prático de usar. No entanto, se você é um especialista em front-end ou possui suas próprias preferências, sinta-se à vontade para usar a ferramenta de sua escolha!
