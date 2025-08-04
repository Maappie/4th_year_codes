class PostsController < ApplicationController
  skip_before_action :verify_authenticity_token
  before_action :set_post, only: [:show, :update, :destroy]

  # GET /posts
  def index
    posts = Post.all
    render json: posts
  end

  # GET /posts/:id
  def show
    render json: @post
  end

  # POST /posts
  def create
    Rails.logger.debug "🪵 RAW PARAMS: #{params.inspect}"
    post = Post.new(post_params)
    if post.save
      render json: post, status: :created
    else
      render json: { errors: post.errors.full_messages }, status: :unprocessable_entity
    end
  rescue => e
    File.write("log/api_error.log", "#{Time.now} - #{e.class}: #{e.message}\n#{e.backtrace.join("\n")}")
    Rails.logger.error "🔥 Exception written to log/api_error.log"
    render json: { error: "Something went wrong. Check log/api_error.log" }, status: :internal_server_error
  end


  # PUT /posts/:id
  def update
    if @post.update(post_params)
      render json: @post
    else
      render json: @post.errors, status: :unprocessable_entity
    end
  end

  # DELETE /posts/:id
  def destroy
    @post.destroy
    head :no_content
  end

  private

  def set_post
    @post = Post.find(params[:id])
  end

  def post_params
    params.require(:post).permit(:title, :body)
  end
end
