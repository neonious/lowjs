const webpack = require('webpack');
const path = require('path')
const CleanWebpackPlugin = require('clean-webpack-plugin');
const HtmlWebpackPlugin = require('html-webpack-plugin');
const autoprefixer = require('autoprefixer');
const ExtractTextPlugin = require("extract-text-webpack-plugin");

const mainConfig = (env, options) => {

    const mode = options.mode;
    const isProduction = mode === 'production';
    const outDir = 'build';
    const extractSass = new ExtractTextPlugin({
        filename: `styles.[chunkhash].css`,
        disable: true
    });
    const getExtract = (arg) => {
        return extractSass.extract({
            use: [
                arg,
                {
                    loader: "postcss-loader",
                    options: {
                        plugins: () => [autoprefixer({
                            browsers: [
                                'last 3 version',
                                'ie >= 10'
                            ]
                        })]
                    }
                },
                { loader: "sass-loader" }
            ],
            fallback: "style-loader"
        });
    }
    return {
        entry: {
            main: './main.tsx'
        },
        output: {
            filename: "[name].bundle.js",
            path: path.resolve(`${__dirname}/${outDir}`)
        },
        resolve: {
            extensions: [".ts", ".tsx", ".js", ".json"]
        },
        module: {
            rules: [
                {
                    test: /\.tsx?$/,
                    loader: "ts-loader"
                },
                {
                    test: /\.(css|sass|scss)$/,
                    use: getExtract({
                        loader: "typings-for-css-modules-loader",
                        options: {
                            namedExport: true,
                            camelCase: true,
                            modules: true,
                            localIdentName: '[local]--[hash:base64:5]',
                            minimize: isProduction
                        }
                    })
                },
            ]
        },
        plugins: [
            new CleanWebpackPlugin(outDir),
            new HtmlWebpackPlugin({
                title: 'Test results',
                filename: 'index.html',
                minify: isProduction ? {
                    collapseWhitespace: true,
                    collapseInlineTagWhitespace: true,
                    removeComments: true,
                    removeRedundantAttributes: true
                } : false,
                template: 'index_template.html',
                isProduction
            }),
            new webpack.WatchIgnorePlugin([
                /scss\.d\.ts$/
            ]),
            extractSass
        ]
    };
}

module.exports = mainConfig;