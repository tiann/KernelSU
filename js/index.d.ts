interface ExecOptions {
    cwd?: string,
    env?: { [key: string]: string }
}

interface ExecResults {
    errno: number,
    stdout: string,
    stderr: string
}

declare function exec(command: string): Promise<ExecResults>;
declare function exec(command: string, options: ExecOptions): Promise<ExecResults>;

interface SpawnOptions {
    cwd?: string,
    env?: { [key: string]: string }
}

interface Stdio {
    on(event: 'data', callback: (data: string) => void)
}

interface ChildProcess {
    stdout: Stdio,
    stderr: Stdio,
    on(event: 'exit', callback: (code: number) => void)
    on(event: 'error', callback: (err: any) => void)
}

declare function spawn(command: string): ChildProcess;
declare function spawn(command: string, args: string[]): ChildProcess;
declare function spawn(command: string, options: SpawnOptions): ChildProcess;
declare function spawn(command: string, args: string[], options: SpawnOptions): ChildProcess;

declare function fullScreen(isFullScreen: boolean);

declare function toast(message: string);

declare function moduleInfo(): string;

export {
    exec,
    spawn,
    fullScreen,
    toast,
    moduleInfo
}
