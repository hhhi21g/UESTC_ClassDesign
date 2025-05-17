import Thymeleaf.viewBaseServlet;

import javax.servlet.ServletException;
import javax.servlet.annotation.WebServlet;
import javax.servlet.http.HttpServletRequest;
import javax.servlet.http.HttpServletResponse;
import javax.servlet.http.HttpSession;
import java.io.IOException;
import java.util.List;

@WebServlet("/delete")
public class deleteServlet extends viewBaseServlet {
    @Override
    protected void doPost(HttpServletRequest req, HttpServletResponse resp) throws ServletException, IOException {
        HttpSession session = req.getSession();
        req.setCharacterEncoding("utf-8");
        resp.setCharacterEncoding("utf-8");
        resp.setContentType("text/html;charset=UTF-8");
        System.out.println("/upload");
        System.out.println("当前Session ID:"+session.getId());
        System.out.println("当前备忘录列表："+session.getAttribute("allContents"));
        System.out.println("请求Cookie:"+req.getHeader("Cookie"));
        System.out.println("*********************************************");

        String content = req.getParameter("note");
        System.out.println(content);

        List<String> allContents = (List<String>) session.getAttribute("allContents");

        if (allContents != null && content != null) {
            allContents.remove(content); // 删除指定内容
            session.setAttribute("allContents", allContents);
            req.setAttribute("allContents",allContents);
        }
        super.processTemplate("index",req,resp);
    }
}
