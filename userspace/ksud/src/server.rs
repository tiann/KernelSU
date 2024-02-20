use anyhow::Result;
use axum::Router;
use std::net::SocketAddr;
use std::path::Path;
use tower_http::services::ServeDir;

use crate::defs;

async fn serve(dir: impl AsRef<Path>, port: u16) {
    let app = Router::new().nest_service("/", ServeDir::new(dir));
    let addr = SocketAddr::from(([127, 0, 0, 1], port));
    let listener = tokio::net::TcpListener::bind(addr).await.unwrap();
    axum::serve(listener, app).await.unwrap();
}

pub fn serve_module(id: &str, port: u16) -> Result<()> {
    let module_dir = std::path::PathBuf::from(defs::MODULE_DIR)
        .join(id)
        .join(defs::MODULE_WEB_DIR);
    serve_dir(module_dir, port)
}

pub fn serve_dir(dir: impl AsRef<Path>, port: u16) -> Result<()> {
    let rt = tokio::runtime::Runtime::new()?;
    rt.block_on(serve(dir, port));
    Ok(())
}
