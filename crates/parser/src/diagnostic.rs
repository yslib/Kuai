#[derive(Debug, Clone)]
pub enum Severity {
    Error,
    Warning,
    Hint,
}

#[derive(Clone)]
pub struct Diagnostic {
    pub severity: Severity,
    pub span: std::ops::Range<usize>,
    pub message: String,
}

impl Diagnostic {
    pub fn error(severity: Severity, span: std::ops::Range<usize>, message: String) -> Self {
        Self {
            severity,
            span,
            message,
        }
    }
}

impl std::fmt::Display for Diagnostic {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(
            f,
            "[{:?}] at {:?}: {}",
            self.severity, self.span, self.message
        )
    }
}
