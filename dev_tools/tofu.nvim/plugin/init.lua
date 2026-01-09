vim.filetype.add({
    extension = {
        tof = "tofu",
    },
})

require("tofu").setup()
require("tofu.config")
require("tofu.repl")
vim.notify("Tofu.nvim loaded!", vim.log.levels.INFO, { title = "Tofu.nvim" })
