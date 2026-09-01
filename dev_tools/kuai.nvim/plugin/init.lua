vim.filetype.add({
    extension = {
        ku = "kuai",
    },
})

require("kuai").setup()
require("kuai.config")
require("kuai.repl")
vim.notify("Kuai.nvim loaded!", vim.log.levels.INFO, { title = "Kuai.nvim" })
