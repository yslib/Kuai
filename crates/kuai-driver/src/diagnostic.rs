//! Driver diagnostics, including explicit conversions from frontend stages.

#[derive(Debug, Clone)]
pub enum Severity {
    Error,
    Warning,
    Hint,
}

#[derive(Clone, Debug)]
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

/// Resilient compilation result that contains both the output and diagnostics
/// This allows for partial success - we can have a result even with errors
#[derive(Debug, Clone)]
pub struct CompileResult<T> {
    /// The compilation output (may be partial if there were errors)
    pub output: Option<T>,
    /// All diagnostics collected during compilation (errors, warnings, hints)
    pub diagnostics: Vec<Diagnostic>,
}

impl<T> CompileResult<T> {
    /// Create a successful result with no diagnostics
    pub fn ok(output: T) -> Self {
        Self {
            output: Some(output),
            diagnostics: Vec::new(),
        }
    }

    /// Create a result with output and diagnostics
    pub fn with_diagnostics(output: T, diagnostics: Vec<Diagnostic>) -> Self {
        Self {
            output: Some(output),
            diagnostics,
        }
    }

    /// Create a failed result with only diagnostics
    pub fn err(diagnostics: Vec<Diagnostic>) -> Self {
        Self {
            output: None,
            diagnostics,
        }
    }

    /// Check if there are any errors (not just warnings)
    pub fn has_errors(&self) -> bool {
        self.diagnostics
            .iter()
            .any(|d| matches!(d.severity, Severity::Error))
    }

    /// Check if compilation was successful (has output and no errors)
    pub fn is_ok(&self) -> bool {
        self.output.is_some() && !self.has_errors()
    }

    /// Get only errors from diagnostics
    pub fn errors(&self) -> Vec<&Diagnostic> {
        self.diagnostics
            .iter()
            .filter(|d| matches!(d.severity, Severity::Error))
            .collect()
    }

    /// Get only warnings from diagnostics
    pub fn warnings(&self) -> Vec<&Diagnostic> {
        self.diagnostics
            .iter()
            .filter(|d| matches!(d.severity, Severity::Warning))
            .collect()
    }

    /// Map the output type while preserving diagnostics
    pub fn map<U, F>(self, f: F) -> CompileResult<U>
    where
        F: FnOnce(T) -> U,
    {
        CompileResult {
            output: self.output.map(f),
            diagnostics: self.diagnostics,
        }
    }

    /// Combine diagnostics from another result
    pub fn merge_diagnostics(&mut self, other: Vec<Diagnostic>) {
        self.diagnostics.extend(other);
    }
}

impl<T> Default for CompileResult<T> {
    fn default() -> Self {
        Self {
            output: None,
            diagnostics: Vec::new(),
        }
    }
}

impl From<syntax::diagnostic::Diagnostic> for Diagnostic {
    fn from(value: syntax::diagnostic::Diagnostic) -> Self {
        let severity = match value.severity {
            syntax::diagnostic::Severity::Error => Severity::Error,
            syntax::diagnostic::Severity::Warning => Severity::Warning,
            syntax::diagnostic::Severity::Hint => Severity::Hint,
        };
        Self {
            severity,
            span: value.span,
            message: value.message,
        }
    }
}

impl From<sema::diagnostic::Diagnostic> for Diagnostic {
    fn from(value: sema::diagnostic::Diagnostic) -> Self {
        let severity = match value.severity {
            sema::diagnostic::Severity::Error => Severity::Error,
            sema::diagnostic::Severity::Warning => Severity::Warning,
            sema::diagnostic::Severity::Hint => Severity::Hint,
        };
        Self {
            severity,
            span: value.span,
            message: value.message,
        }
    }
}
