#[derive(Debug, Clone)]
pub enum Serverity {
    Error,
    Warning,
    Hint,
}

#[derive(Clone)]
pub struct Diagnostic {
    pub serverity: Serverity,
    pub span: std::ops::Range<usize>,
    pub message: String,

    #[cfg(debug_assertions)]
    backtrace: Option<String>,
}

impl Diagnostic {
    pub fn error(serverity: Serverity, span: std::ops::Range<usize>, message: String) -> Self {
        Self {
            serverity,
            span,
            message,
            #[cfg(debug_assertions)]
            backtrace: Some(format!("{:?}", std::backtrace::Backtrace::capture())),
        }
    }
}

impl std::fmt::Display for Diagnostic {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(
            f,
            "[{:?}] at {:?}: {}",
            self.serverity, self.span, self.message
        )
    }
}

impl std::fmt::Debug for Diagnostic {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(
            f,
            "[{:?}] at {:?}: {}",
            self.serverity, self.span, self.message
        )?;
        #[cfg(debug_assertions)]
        if let Some(bt) = &self.backtrace {
            write!(f, "\nBacktrace:\n{}", bt)?;
        }
        Ok(())
    }
}
