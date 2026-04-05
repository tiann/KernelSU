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

declare function enableEdgeToEdge(enable: boolean);

declare function toast(message: string);

declare function moduleInfo(): string;

interface PackagesInfo {
    packageName: string;
    versionName: string;
    versionCode: number;
    appLabel: string;
    isSystem: boolean;
    uid: number;
}

declare function listPackages(type: string): string[];

declare function getPackagesInfo(packages: string[]): PackagesInfo[];

declare function exit();

interface FileInstance {
    exists(): boolean;
    isFile(): boolean;
    isDirectory(): boolean;
    canRead(): boolean;
    canWrite(): boolean;
    canExecute(): boolean;
    createNewFile(): boolean;
    delete(): boolean;
    deleteRecursive(): boolean;
    mkdir(): boolean;
    mkdirs(): boolean;
    renameTo(destPath: string): boolean;
    list(): string[];
    listFiles(): string[];
    length(): number;
    lastModified(): number;
    setLastModified(time: number): boolean;
    getAbsolutePath(): string;
    getCanonicalPath(): string;
    getParent(): string | null;
    getPath(): string;
    getName(): string;
    isHidden(): boolean;
    isBlock(): boolean;
    isCharacter(): boolean;
    isSymlink(): boolean;
    createNewSymlink(target: string): boolean;
    createNewLink(existing: string): boolean;
    clear(): boolean;
    setReadOnly(): boolean;
    setReadable(readable: boolean, ownerOnly: boolean): boolean;
    setWritable(writable: boolean, ownerOnly: boolean): boolean;
    setExecutable(executable: boolean, ownerOnly: boolean): boolean;
    getFreeSpace(): number;
    getTotalSpace(): number;
    getUsableSpace(): number;
    newInputStream(): string;
    newOutputStream(append?: boolean): string;
    toString(): string;
}

interface FileInputStreamInstance {
    open(path: string): string;
    read(id: string): string;
    read(id: string, maxBytes: number): string;
    available(id: string): number;
    close(id: string): boolean;
}

interface FileOutputStreamInstance {
    open(path: string, append?: boolean): string;
    write(id: string, base64: string): boolean;
    writeByte(id: string, b: number): boolean;
    flush(id: string): boolean;
    close(id: string): boolean;
}

interface RandomAccessFileInstance {
    open(path: string, mode: string): string;
    read(id: string): number;
    readBytes(id: string, len: number): string;
    readBoolean(id: string): boolean;
    readByte(id: string): number;
    readInt(id: string): number;
    readLong(id: string): number;
    readShort(id: string): number;
    readFloat(id: string): number;
    readDouble(id: string): number;
    readUTF(id: string): string;
    readLine(id: string): string | null;
    write(id: string, b: number): void;
    writeBase64(id: string, data: string): void;
    writeBoolean(id: string, v: boolean): void;
    writeByte(id: string, v: number): void;
    writeInt(id: string, v: number): void;
    writeLong(id: string, v: number): void;
    writeShort(id: string, v: number): void;
    writeFloat(id: string, v: number): void;
    writeDouble(id: string, v: number): void;
    writeUTF(id: string, str: string): void;
    seek(id: string, pos: number): boolean;
    getFilePointer(id: string): number;
    length(id: string): number;
    setLength(id: string, newLength: number): boolean;
    close(id: string): boolean;
}

interface KsuIO {
    File(path: string): FileInstance;
    FileInputStream(): FileInputStreamInstance;
    FileOutputStream(): FileOutputStreamInstance;
    RandomAccessFile(): RandomAccessFileInstance;
}

declare const io: KsuIO;

export {
    exec,
    spawn,
    fullScreen,
    enableEdgeToEdge,
    toast,
    moduleInfo,
    listPackages,
    getPackagesInfo,
    exit,
    io,
}
