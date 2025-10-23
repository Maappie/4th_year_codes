class MessagesController < ApplicationController
  def index
    @messages = Message.order(created_at: :desc).limit(200)
  end

  def show
    @message = Message.find(params[:id])
  end
end
